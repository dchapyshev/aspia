//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/sha512.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "crypto/sha512.h"

#include <algorithm>

#include "base/exception.h"
#include "base/logging.h"

namespace aspia {

#define ROR64( value, bits ) (((value) >> (bits)) | ((value) << (64 - (bits))))

#define LOAD64H( x, y )                                                                \
    {                                                                                  \
        x = (((uint64_t)((y)[0] & 255)) << 56) | (((uint64_t)((y)[1] & 255)) << 48) |  \
            (((uint64_t)((y)[2] & 255)) << 40) | (((uint64_t)((y)[3] & 255)) << 32) |  \
            (((uint64_t)((y)[4] & 255)) << 24) | (((uint64_t)((y)[5] & 255)) << 16) |  \
            (((uint64_t)((y)[6] & 255)) << 8)  | (((uint64_t)((y)[7] & 255)));         \
    }

#define STORE64H( x, y )                                                               \
    {                                                                                  \
        (y)[0] = (uint8_t)(((x) >> 56) & 255); (y)[1] = (uint8_t)(((x) >> 48) & 255);  \
        (y)[2] = (uint8_t)(((x) >> 40) & 255); (y)[3] = (uint8_t)(((x) >> 32) & 255);  \
        (y)[4] = (uint8_t)(((x) >> 24) & 255); (y)[5] = (uint8_t)(((x) >> 16) & 255);  \
        (y)[6] = (uint8_t)(((x) >> 8)  & 255); (y)[7] = (uint8_t)((x)         & 255);  \
    }

#define CH( x, y, z )     (z ^ (x & (y ^ z)))
#define MAJ(x, y, z )     (((x | y) & z) | (x & y))
#define S( x, n )         ROR64( x, n )
#define R( x, n )         (((x) & 0xFFFFFFFFFFFFFFFFULL) >> ((uint64_t)n))
#define SIGMA0( x )       (S(x, 28) ^ S(x, 34) ^ S(x, 39))
#define SIGMA1( x )       (S(x, 14) ^ S(x, 18) ^ S(x, 41))
#define GAMMA0( x )       (S(x, 1)  ^ S(x, 8)  ^ R(x, 7))
#define GAMMA1( x )       (S(x, 19) ^ S(x, 61) ^ R(x, 6))

#define ROUND( a, b, c, d, e, f, g, h, i )             \
     t0 = h + SIGMA1(e) + CH(e, f, g) + K[i] + W[i];   \
     t1 = SIGMA0(a) + MAJ(a, b, c);                    \
     d += t0;                                          \
     h  = t0 + t1;

// The K array
static const uint64_t K[80] =
{
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static const size_t kBlockSize = 128;

SHA512::SHA512()
{
    memset(buf_, 0, sizeof(buf_));

    curlen_ = 0;
    length_ = 0;

    state_[0] = 0x6a09e667f3bcc908ULL;
    state_[1] = 0xbb67ae8584caa73bULL;
    state_[2] = 0x3c6ef372fe94f82bULL;
    state_[3] = 0xa54ff53a5f1d36f1ULL;
    state_[4] = 0x510e527fade682d1ULL;
    state_[5] = 0x9b05688c2b3e6c1fULL;
    state_[6] = 0x1f83d9abfb41bd6bULL;
    state_[7] = 0x5be0cd19137e2179ULL;
}

SHA512::~SHA512()
{
    // Nothing
}

void SHA512::Transform(const uint8_t *buffer)
{
    uint64_t S[8];
    uint64_t W[80];
    uint64_t t0;
    uint64_t t1;

    // Copy state into S
    for (int i = 0; i < 8; ++i)
    {
        S[i] = state_[i];
    }

    // Copy the state into 1024-bits into W[0..15]
    for (int i = 0; i < 16; ++i)
    {
        LOAD64H(W[i], buffer + (8 * i));
    }

    // Fill W[16..79]
    for (int i = 16; i < 80; ++i)
    {
        W[i] = GAMMA1(W[i - 2]) + W[i - 7] + GAMMA0(W[i - 15]) + W[i - 16];
    }

    // Compress
    for (int i = 0; i < 80; i += 8)
    {
        ROUND(S[0], S[1], S[2], S[3], S[4], S[5], S[6], S[7], i + 0);
        ROUND(S[7], S[0], S[1], S[2], S[3], S[4], S[5], S[6], i + 1);
        ROUND(S[6], S[7], S[0], S[1], S[2], S[3], S[4], S[5], i + 2);
        ROUND(S[5], S[6], S[7], S[0], S[1], S[2], S[3], S[4], i + 3);
        ROUND(S[4], S[5], S[6], S[7], S[0], S[1], S[2], S[3], i + 4);
        ROUND(S[3], S[4], S[5], S[6], S[7], S[0], S[1], S[2], i + 5);
        ROUND(S[2], S[3], S[4], S[5], S[6], S[7], S[0], S[1], i + 6);
        ROUND(S[1], S[2], S[3], S[4], S[5], S[6], S[7], S[0], i + 7);
    }

    // Feedback
    for (int i = 0; i < 8; ++i)
    {
        state_[i] += S[i];
    }
}

void SHA512::Update(const uint8_t *buffer, size_t size)
{
    if (curlen_ > sizeof(buf_))
    {
        return;
    }

    while (size > 0)
    {
        if (curlen_ == 0 && size >= kBlockSize)
        {
            Transform(buffer);

            length_ += kBlockSize * 8;
            buffer = buffer + kBlockSize;
            size -= kBlockSize;
        }
        else
        {
            size_t n = std::min(size, (kBlockSize - curlen_));

            memcpy(buf_ + curlen_, buffer, n);

            curlen_ += n;
            buffer = buffer + n;
            size -= n;

            if (curlen_ == kBlockSize)
            {
                Transform(buf_);
                length_ += 8 * kBlockSize;
                curlen_ = 0;
            }
        }
    }
}

void SHA512::Update(const std::string &buffer)
{
    Update(reinterpret_cast<const uint8_t*>(buffer.c_str()), buffer.size());
}

void SHA512::Final(Hash *hash)
{
    if (curlen_ >= sizeof(buf_))
    {
        return;
    }

    // Increase the length of the message
    length_ += curlen_ * 8ULL;

    // Append the '1' bit
    buf_[curlen_++] = 0x80;

    // If the length is currently above 112 bytes we append zeros
    // then compress.  Then we can fall back to padding zeros and length
    // encoding like normal.
    if (curlen_ > 112)
    {
        while (curlen_ < 128)
        {
            buf_[curlen_++] = 0;
        }
        Transform(buf_);
        curlen_ = 0;
    }

    // Pad up to 120 bytes of zeroes
    // note: that from 112 to 120 is the 64 MSB of the length.  We assume that you won't hash
    // > 2^64 bits of data... :-)
    while (curlen_ < 120)
    {
        buf_[curlen_++] = 0;
    }

    // Store length
    STORE64H(length_, buf_ + 120);
    Transform(buf_);

    // Copy output
    for (int i = 0; i < 8; ++i)
    {
        STORE64H(state_[i], hash->data + (8 * i));
    }

    hash->size = sizeof(hash->data);
}

} // namespace aspia
