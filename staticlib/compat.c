#include <string.h>

__attribute__((weak)) size_t strlcpy(char *dst, const char *src, size_t size) {
	size_t srclen = strlen(src);
	size_t cpylen = (srclen < size) ? srclen : (size - 1);

	memcpy(dst, src, cpylen);
	dst[cpylen] = 0;

	return srclen;
}

__attribute__((weak)) size_t strlcat(char *dst, const char *src, size_t size) {
	size_t dstlen = strlen(dst);
	size_t srclen = strlen(src);
	size_t cpylen;

	if (dstlen < size) {
		cpylen = size - dstlen;
		if (cpylen > srclen) cpylen = srclen;
		memcpy(dst + dstlen, src, cpylen);
		dst[cpylen] = 0;
	}

	return dstlen + srclen;
}
