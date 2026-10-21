#ifndef _MD5_HPP_
#define _MD5_HPP_

#include <stdio.h>
#include <memory>
#include <string>

class CMD5Tool
{
private:
#define MD5_F(x,y,z) ((x & y) | (~x & z))    
#define MD5_G(x,y,z) ((x & z) | (y & ~z))    
#define MD5_H(x,y,z) (x^y^z)    
#define MD5_I(x,y,z) (y ^ (x | ~z))    
#define ROTATE_LEFT(x,n) ((x << n) | (x >> (32-n)))    

#define MD5_FF(a,b,c,d,x,s,ac) {\
a += MD5_F(b, c, d) + x + ac;\
	a = ROTATE_LEFT(a, s);\
	a += b;\
	}

#define MD5_GG(a,b,c,d,x,s,ac) {\
a += MD5_G(b, c, d) + x + ac;\
	a = ROTATE_LEFT(a, s);\
	a += b;\
	}

#define MD5_HH(a,b,c,d,x,s,ac) {\
a += MD5_H(b, c, d) + x + ac; \
	a = ROTATE_LEFT(a, s); \
	a += b; \
	}  
#define MD5_II(a,b,c,d,x,s,ac) { \
a += MD5_I(b, c, d) + x + ac; \
	a = ROTATE_LEFT(a, s); \
	a += b; \
	}  
public:
	typedef struct
	{
		unsigned int count[2];
		unsigned int state[4];
		unsigned char buffer[64];
	}MD5_CTX;
	//在逆向代码的时候，需要关注下面的特征值  
	static void MD5Init(MD5_CTX *context)
	{
		context->count[0] = 0;
		context->count[1] = 0;
		context->state[0] = 0x67452301;
		context->state[1] = 0xEFCDAB89;
		context->state[2] = 0x98BADCFE;
		context->state[3] = 0x10325476;
	}

	static void MD5Encode(unsigned char *output, unsigned int *input, unsigned int len)
	{
		unsigned int i = 0, j = 0;
		while (j < len)
		{
			output[j] = input[i] & 0xFF;
			output[j + 1] = (input[i] >> 8) & 0xFF;
			output[j + 2] = (input[i] >> 16) & 0xFF;
			output[j + 3] = (input[i] >> 24) & 0xFF;
			i++;
			j += 4;
		}
	}

	static void MD5Decode(unsigned int *output, unsigned char *input, unsigned int len)
	{
		unsigned int i = 0, j = 0;
		while (j < len)
		{
			output[i] = (input[j]) |
				(input[j + 1] << 8) |
				(input[j + 2] << 16) |
				(input[j + 3] << 24);
			i++;
			j += 4;
		}
	}

	static void MD5Transform(unsigned int state[4], unsigned char block[64])
	{
		unsigned int a = state[0];
		unsigned int b = state[1];
		unsigned int c = state[2];
		unsigned int d = state[3];
		unsigned int x[64];

		MD5Decode(x, block, 64);
		MD5_FF(a, b, c, d, x[0], 7, 0xd76aa478);
		MD5_FF(d, a, b, c, x[1], 12, 0xe8c7b756);
		MD5_FF(c, d, a, b, x[2], 17, 0x242070db);
		MD5_FF(b, c, d, a, x[3], 22, 0xc1bdceee);
		MD5_FF(a, b, c, d, x[4], 7, 0xf57c0faf);
		MD5_FF(d, a, b, c, x[5], 12, 0x4787c62a);
		MD5_FF(c, d, a, b, x[6], 17, 0xa8304613);
		MD5_FF(b, c, d, a, x[7], 22, 0xfd469501);
		MD5_FF(a, b, c, d, x[8], 7, 0x698098d8);
		MD5_FF(d, a, b, c, x[9], 12, 0x8b44f7af);
		MD5_FF(c, d, a, b, x[10], 17, 0xffff5bb1);
		MD5_FF(b, c, d, a, x[11], 22, 0x895cd7be);
		MD5_FF(a, b, c, d, x[12], 7, 0x6b901122);
		MD5_FF(d, a, b, c, x[13], 12, 0xfd987193);
		MD5_FF(c, d, a, b, x[14], 17, 0xa679438e);
		MD5_FF(b, c, d, a, x[15], 22, 0x49b40821);


		MD5_GG(a, b, c, d, x[1], 5, 0xf61e2562);
		MD5_GG(d, a, b, c, x[6], 9, 0xc040b340);
		MD5_GG(c, d, a, b, x[11], 14, 0x265e5a51);
		MD5_GG(b, c, d, a, x[0], 20, 0xe9b6c7aa);
		MD5_GG(a, b, c, d, x[5], 5, 0xd62f105d);
		MD5_GG(d, a, b, c, x[10], 9, 0x2441453);
		MD5_GG(c, d, a, b, x[15], 14, 0xd8a1e681);
		MD5_GG(b, c, d, a, x[4], 20, 0xe7d3fbc8);
		MD5_GG(a, b, c, d, x[9], 5, 0x21e1cde6);
		MD5_GG(d, a, b, c, x[14], 9, 0xc33707d6);
		MD5_GG(c, d, a, b, x[3], 14, 0xf4d50d87);
		MD5_GG(b, c, d, a, x[8], 20, 0x455a14ed);
		MD5_GG(a, b, c, d, x[13], 5, 0xa9e3e905);
		MD5_GG(d, a, b, c, x[2], 9, 0xfcefa3f8);
		MD5_GG(c, d, a, b, x[7], 14, 0x676f02d9);
		MD5_GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);


		MD5_HH(a, b, c, d, x[5], 4, 0xfffa3942);
		MD5_HH(d, a, b, c, x[8], 11, 0x8771f681);
		MD5_HH(c, d, a, b, x[11], 16, 0x6d9d6122);
		MD5_HH(b, c, d, a, x[14], 23, 0xfde5380c);
		MD5_HH(a, b, c, d, x[1], 4, 0xa4beea44);
		MD5_HH(d, a, b, c, x[4], 11, 0x4bdecfa9);
		MD5_HH(c, d, a, b, x[7], 16, 0xf6bb4b60);
		MD5_HH(b, c, d, a, x[10], 23, 0xbebfbc70);
		MD5_HH(a, b, c, d, x[13], 4, 0x289b7ec6);
		MD5_HH(d, a, b, c, x[0], 11, 0xeaa127fa);
		MD5_HH(c, d, a, b, x[3], 16, 0xd4ef3085);
		MD5_HH(b, c, d, a, x[6], 23, 0x4881d05);
		MD5_HH(a, b, c, d, x[9], 4, 0xd9d4d039);
		MD5_HH(d, a, b, c, x[12], 11, 0xe6db99e5);
		MD5_HH(c, d, a, b, x[15], 16, 0x1fa27cf8);
		MD5_HH(b, c, d, a, x[2], 23, 0xc4ac5665);


		MD5_II(a, b, c, d, x[0], 6, 0xf4292244);
		MD5_II(d, a, b, c, x[7], 10, 0x432aff97);
		MD5_II(c, d, a, b, x[14], 15, 0xab9423a7);
		MD5_II(b, c, d, a, x[5], 21, 0xfc93a039);
		MD5_II(a, b, c, d, x[12], 6, 0x655b59c3);
		MD5_II(d, a, b, c, x[3], 10, 0x8f0ccc92);
		MD5_II(c, d, a, b, x[10], 15, 0xffeff47d);
		MD5_II(b, c, d, a, x[1], 21, 0x85845dd1);
		MD5_II(a, b, c, d, x[8], 6, 0x6fa87e4f);
		MD5_II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
		MD5_II(c, d, a, b, x[6], 15, 0xa3014314);
		MD5_II(b, c, d, a, x[13], 21, 0x4e0811a1);
		MD5_II(a, b, c, d, x[4], 6, 0xf7537e82);
		MD5_II(d, a, b, c, x[11], 10, 0xbd3af235);
		MD5_II(c, d, a, b, x[2], 15, 0x2ad7d2bb);
		MD5_II(b, c, d, a, x[9], 21, 0xeb86d391);
		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
	}
	static void MD5Update(MD5_CTX *context, unsigned char *input, unsigned int inputlen)
	{
		unsigned int i = 0, index = 0, partlen = 0;
		index = (context->count[0] >> 3) & 0x3F;
		partlen = 64 - index;
		context->count[0] += inputlen << 3;
		if (context->count[0] < (inputlen << 3))
			context->count[1]++;
		context->count[1] += inputlen >> 29;

		if (inputlen >= partlen)
		{
			memcpy(&context->buffer[index], input, partlen);
			MD5Transform(context->state, context->buffer);
			for (i = partlen; i + 64 <= inputlen; i += 64)
				MD5Transform(context->state, &input[i]);
			index = 0;
		}
		else
		{
			i = 0;
		}
		memcpy(&context->buffer[index], &input[i], inputlen - i);
	}

	static void MD5Final(MD5_CTX *context, string& strDigest)
	{
		unsigned char PADDING[] = {
			0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		unsigned char szDigest[16] = {};
		unsigned int index = 0, padlen = 0;
		unsigned char bits[8];
		index = (context->count[0] >> 3) & 0x3F;
		padlen = (index < 56) ? (56 - index) : (120 - index);
		MD5Encode(bits, context->count, 8);
		MD5Update(context, PADDING, padlen);
		MD5Update(context, bits, 8);
		MD5Encode(szDigest, context->state, 16);
		char szHex[4] = {};
		for (int i = 0; i<16; ++i)
		{
			sprintf_s(szHex, "%02X", szDigest[i]);
			strDigest += szHex;
		}
	}
	/**
	* \brief 计算指定文件的MD5值
	* \param file_path 文件路径
	* \return md5字符串
	*/
	static std::string CalcMD5(std::string file_path)
	{
		FILE *pFile = nullptr;
		fopen_s(&pFile, file_path.c_str(), "rb");
		if (pFile)
		{
			fseek(pFile, 0, SEEK_END);
			long lSize = ftell(pFile);
			fseek(pFile, 0, SEEK_SET);
			unsigned char *pBuf = new unsigned char[lSize];
			fread(pBuf, 1, lSize, pFile);
			fclose(pFile);
			pFile = nullptr;

			CMD5Tool::MD5_CTX ctx;
			CMD5Tool::MD5Init(&ctx);
			CMD5Tool::MD5Update(&ctx, pBuf, lSize);
			string strMd5;
			CMD5Tool::MD5Final(&ctx, strMd5);

			delete pBuf;
			pBuf = nullptr;

			return strMd5;
		}
		return "";
	}
};

#endif
