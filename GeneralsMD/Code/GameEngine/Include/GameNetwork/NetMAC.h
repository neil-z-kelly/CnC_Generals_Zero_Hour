/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// NetMAC.h //////////////////////////////////////////////////////////////////
// Keyed message authentication for the game transport layer.
//
// The transport used to accept a datagram based on a fixed XOR keystream, a
// constant magic number and an unkeyed CRC.  All three are reproducible by
// anyone, so they authenticate nothing.  These helpers provide the keyed
// primitive (HMAC-SHA1) used to authenticate datagrams with a per-session
// secret, plus the session key generation and constant time comparison used
// along with it.
//
// Header only so it can be used from the game engine and from tools without
// touching the project files.

#pragma once

#ifndef _NETMAC_H_
#define _NETMAC_H_

#include <string.h>
#include <time.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "Lib/BaseType.h"

/// Length of the per-session shared secret used to key the packet MAC.  Kept
/// small because the key travels as hex inside the game options string, which
/// has to fit in a single LAN broadcast packet.
static const Int NET_SESSION_KEY_LEN = 12;

/// Number of MAC bytes carried in each transport packet header.
static const Int NET_MAC_LEN = 8;

static const Int NET_SHA1_DIGEST_LEN = 20;
static const Int NET_SHA1_BLOCK_LEN = 64;

struct NetSHA1Context
{
	UnsignedInt state[5];
	UnsignedInt count;			///< message length in bytes
	UnsignedByte buffer[NET_SHA1_BLOCK_LEN];
};

inline UnsignedInt netSHA1Rotate(UnsignedInt value, UnsignedInt bits)
{
	return (value << bits) | (value >> (32 - bits));
}

inline void netSHA1Transform(UnsignedInt state[5], const UnsignedByte block[NET_SHA1_BLOCK_LEN])
{
	UnsignedInt w[80];
	Int i;

	for (i = 0; i < 16; ++i)
	{
		w[i] = ((UnsignedInt)block[i * 4] << 24) | ((UnsignedInt)block[i * 4 + 1] << 16) |
			((UnsignedInt)block[i * 4 + 2] << 8) | ((UnsignedInt)block[i * 4 + 3]);
	}
	for (i = 16; i < 80; ++i)
	{
		w[i] = netSHA1Rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	}

	UnsignedInt a = state[0];
	UnsignedInt b = state[1];
	UnsignedInt c = state[2];
	UnsignedInt d = state[3];
	UnsignedInt e = state[4];

	for (i = 0; i < 80; ++i)
	{
		UnsignedInt f, k;
		if (i < 20)
		{
			f = (b & c) | ((~b) & d);
			k = 0x5A827999;
		}
		else if (i < 40)
		{
			f = b ^ c ^ d;
			k = 0x6ED9EBA1;
		}
		else if (i < 60)
		{
			f = (b & c) | (b & d) | (c & d);
			k = 0x8F1BBCDC;
		}
		else
		{
			f = b ^ c ^ d;
			k = 0xCA62C1D6;
		}

		UnsignedInt temp = netSHA1Rotate(a, 5) + f + e + k + w[i];
		e = d;
		d = c;
		c = netSHA1Rotate(b, 30);
		b = a;
		a = temp;
	}

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
}

inline void netSHA1Init(NetSHA1Context *ctx)
{
	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xEFCDAB89;
	ctx->state[2] = 0x98BADCFE;
	ctx->state[3] = 0x10325476;
	ctx->state[4] = 0xC3D2E1F0;
	ctx->count = 0;
	memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

inline void netSHA1Update(NetSHA1Context *ctx, const void *data, Int len)
{
	const UnsignedByte *in = (const UnsignedByte *)data;
	Int used = (Int)(ctx->count % NET_SHA1_BLOCK_LEN);

	ctx->count += (UnsignedInt)len;

	while (len > 0)
	{
		Int space = NET_SHA1_BLOCK_LEN - used;
		Int chunk = (len < space) ? len : space;

		memcpy(ctx->buffer + used, in, chunk);
		used += chunk;
		in += chunk;
		len -= chunk;

		if (used == NET_SHA1_BLOCK_LEN)
		{
			netSHA1Transform(ctx->state, ctx->buffer);
			used = 0;
		}
	}
}

inline void netSHA1Final(NetSHA1Context *ctx, UnsignedByte digest[NET_SHA1_DIGEST_LEN])
{
	UnsignedInt bitCountHigh = ctx->count >> 29;
	UnsignedInt bitCountLow = ctx->count << 3;
	UnsignedByte lengthBytes[8];
	Int i;

	for (i = 0; i < 4; ++i)
	{
		lengthBytes[i] = (UnsignedByte)((bitCountHigh >> ((3 - i) * 8)) & 0xFF);
		lengthBytes[i + 4] = (UnsignedByte)((bitCountLow >> ((3 - i) * 8)) & 0xFF);
	}

	UnsignedByte pad = 0x80;
	netSHA1Update(ctx, &pad, 1);

	pad = 0x00;
	while ((ctx->count % NET_SHA1_BLOCK_LEN) != 56)
	{
		netSHA1Update(ctx, &pad, 1);
	}

	netSHA1Update(ctx, lengthBytes, 8);

	for (i = 0; i < 5; ++i)
	{
		digest[i * 4] = (UnsignedByte)((ctx->state[i] >> 24) & 0xFF);
		digest[i * 4 + 1] = (UnsignedByte)((ctx->state[i] >> 16) & 0xFF);
		digest[i * 4 + 2] = (UnsignedByte)((ctx->state[i] >> 8) & 0xFF);
		digest[i * 4 + 3] = (UnsignedByte)(ctx->state[i] & 0xFF);
	}
}

inline void netSHA1(const void *data, Int len, UnsignedByte digest[NET_SHA1_DIGEST_LEN])
{
	NetSHA1Context ctx;
	netSHA1Init(&ctx);
	netSHA1Update(&ctx, data, len);
	netSHA1Final(&ctx, digest);
}

/**
 * HMAC-SHA1 as described by RFC 2104.  The digest is always
 * NET_SHA1_DIGEST_LEN bytes; callers that carry fewer bytes on the wire
 * truncate from the front.
 */
inline void netHMACSHA1(const UnsignedByte *key, Int keyLen, const void *data, Int dataLen,
	UnsignedByte digest[NET_SHA1_DIGEST_LEN])
{
	UnsignedByte paddedKey[NET_SHA1_BLOCK_LEN];
	UnsignedByte innerPad[NET_SHA1_BLOCK_LEN];
	UnsignedByte outerPad[NET_SHA1_BLOCK_LEN];
	UnsignedByte innerDigest[NET_SHA1_DIGEST_LEN];
	NetSHA1Context ctx;
	Int i;

	memset(paddedKey, 0, sizeof(paddedKey));
	if (keyLen > NET_SHA1_BLOCK_LEN)
	{
		netSHA1(key, keyLen, paddedKey);
	}
	else if (keyLen > 0)
	{
		memcpy(paddedKey, key, keyLen);
	}

	for (i = 0; i < NET_SHA1_BLOCK_LEN; ++i)
	{
		innerPad[i] = (UnsignedByte)(paddedKey[i] ^ 0x36);
		outerPad[i] = (UnsignedByte)(paddedKey[i] ^ 0x5C);
	}

	netSHA1Init(&ctx);
	netSHA1Update(&ctx, innerPad, NET_SHA1_BLOCK_LEN);
	netSHA1Update(&ctx, data, dataLen);
	netSHA1Final(&ctx, innerDigest);

	netSHA1Init(&ctx);
	netSHA1Update(&ctx, outerPad, NET_SHA1_BLOCK_LEN);
	netSHA1Update(&ctx, innerDigest, NET_SHA1_DIGEST_LEN);
	netSHA1Final(&ctx, digest);
}

/**
 * Compare two buffers without leaking where they first differ.
 */
inline Bool netSecureCompare(const UnsignedByte *a, const UnsignedByte *b, Int len)
{
	UnsignedByte diff = 0;
	for (Int i = 0; i < len; ++i)
	{
		diff = (UnsignedByte)(diff | (a[i] ^ b[i]));
	}
	return (diff == 0);
}

/**
 * Fill a buffer with a session secret.  Entropy is gathered from whatever the
 * platform offers and folded through SHA1 so that the key does not expose the
 * raw sources.  Each call also mixes in the previous output so keys generated
 * in the same tick differ.
 */
inline void netGenerateSessionKey(UnsignedByte *key, Int keyLen)
{
	static UnsignedByte s_carry[NET_SHA1_DIGEST_LEN] = { 0 };
	static UnsignedInt s_counter = 0;

	struct
	{
		UnsignedByte carry[NET_SHA1_DIGEST_LEN];
		UnsignedInt counter;
		UnsignedInt clockTicks;
		UnsignedInt wallTime;
		UnsignedInt processID;
		UnsignedInt threadID;
		UnsignedInt highResLow;
		UnsignedInt highResHigh;
		UnsignedInt stackAddr;
		UnsignedInt randomValue;
	} pool;

	memset(&pool, 0, sizeof(pool));
	memcpy(pool.carry, s_carry, sizeof(pool.carry));
	pool.counter = ++s_counter;
	pool.clockTicks = (UnsignedInt)clock();
	pool.wallTime = (UnsignedInt)time(NULL);
	pool.stackAddr = (UnsignedInt)(size_t)&pool;
	pool.randomValue = (UnsignedInt)rand();

#ifdef _WIN32
	LARGE_INTEGER perfCounter;
	if (QueryPerformanceCounter(&perfCounter))
	{
		pool.highResLow = (UnsignedInt)perfCounter.LowPart;
		pool.highResHigh = (UnsignedInt)perfCounter.HighPart;
	}
	pool.processID = (UnsignedInt)GetCurrentProcessId();
	pool.threadID = (UnsignedInt)GetCurrentThreadId();
#else
	pool.processID = (UnsignedInt)getpid();
#endif

	UnsignedByte digest[NET_SHA1_DIGEST_LEN];
	Int generated = 0;
	while (generated < keyLen)
	{
		++pool.counter;
		netSHA1(&pool, sizeof(pool), digest);

		Int chunk = keyLen - generated;
		if (chunk > NET_SHA1_DIGEST_LEN)
		{
			chunk = NET_SHA1_DIGEST_LEN;
		}
		memcpy(key + generated, digest, chunk);
		generated += chunk;

		memcpy(pool.carry, digest, sizeof(pool.carry));
	}

	memcpy(s_carry, digest, sizeof(s_carry));
}

#endif // _NETMAC_H_
