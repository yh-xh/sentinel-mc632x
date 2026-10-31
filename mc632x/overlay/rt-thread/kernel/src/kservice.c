/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-03-16     Bernard      the first version
 * 2006-05-25     Bernard      rewrite vsprintf
 * 2006-08-10     Bernard      add rt_show_version
 * 2010-03-17     Bernard      remove rt_strlcpy function
 *                             fix gcc compiling issue.
 * 2010-04-15     Bernard      remove weak definition on ICCM16C compiler
 * 2012-07-18     Arda         add the alignment display for signed integer
 * 2012-11-23     Bernard      fix IAR compiler error.
 * 2012-12-22     Bernard      fix rt_kprintf issue, which found by Grissiom.
 * 2013-06-24     Bernard      remove rt_kprintf if RT_USING_CONSOLE is not defined.
 * 2013-09-24     aozima       make sure the device is in STREAM mode when used by rt_kprintf.
 * 2015-07-06     Bernard      Add rt_assert_handler routine.
 */

#include <rtthread.h>
#include <rthw.h>

#ifdef RT_USING_MODULE
#include <dlmodule.h>
#endif

/* use precision */
#define RT_PRINTF_PRECISION
/* support long long printf */
#define RT_PRINTF_LONGLONG

/**
 * @addtogroup KernelService
 */

/**@{*/

/* global errno in RT-Thread */
static volatile int __rt_errno;

#if defined(RT_USING_DEVICE) && defined(RT_USING_CONSOLE)
static rt_device_t _console_device = RT_NULL;
#endif

extern int addr_is_cached(void *addr);
extern int addr_all_cached(void *addr1, void *addr2);
RT_WEAK void show_func_callchain(void)
{
    return;
}

#define _U  0x01    /* upper */
#define _L  0x02    /* lower */
#define _D  0x04    /* digit */
#define _C  0x08    /* cntrl */
#define _P  0x10    /* punct */
#define _S  0x20    /* white space (space/lf/tab) */
#define _X  0x40    /* hex digit */
#define _SP 0x80    /* hard space (0x20) */

unsigned char _ctype[] = {
_C, _C, _C, _C, _C, _C, _C, _C,            /* 0-7 */
_C, _C|_S, _C|_S, _C|_S, _C|_S, _C|_S, _C, _C,     /* 8-15 */
_C, _C, _C, _C, _C, _C, _C, _C,            /* 16-23 */
_C, _C, _C, _C, _C, _C, _C, _C,            /* 24-31 */
_S|_SP, _P, _P, _P, _P, _P, _P, _P,            /* 32-39 */
_P, _P, _P, _P, _P, _P, _P, _P,            /* 40-47 */
_D, _D, _D, _D, _D, _D, _D, _D,            /* 48-55 */
_D, _D, _P, _P, _P, _P, _P, _P,            /* 56-63 */
_P, _U|_X, _U|_X, _U|_X, _U|_X, _U|_X, _U|_X, _U,  /* 64-71 */
_U, _U, _U, _U, _U, _U, _U, _U,            /* 72-79 */
_U, _U, _U, _U, _U, _U, _U, _U,            /* 80-87 */
_U, _U, _U, _P, _P, _P, _P, _P,            /* 88-95 */
_P, _L|_X, _L|_X, _L|_X, _L|_X, _L|_X, _L|_X, _L,  /* 96-103 */
_L, _L, _L, _L, _L, _L, _L, _L,            /* 104-111 */
_L, _L, _L, _L, _L, _L, _L, _L,            /* 112-119 */
_L, _L, _L, _P, _P, _P, _P, _C,            /* 120-127 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,        /* 128-143 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,        /* 144-159 */
_S|_SP, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P,   /* 160-175 */
_P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P,       /* 176-191 */
_U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U,       /* 192-207 */
_U, _U, _U, _U, _U, _U, _U, _P, _U, _U, _U, _U, _U, _U, _U, _L,       /* 208-223 */
_L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L,       /* 224-239 */
_L, _L, _L, _L, _L, _L, _L, _P, _L, _L, _L, _L, _L, _L, _L, _L};      /* 240-255 */

#define __ismask(x) (_ctype[(int)(unsigned char)(x)])

#define rt_isalnum(c)   ((__ismask(c)&(_U|_L|_D)) != 0)
#define rt_isalpha(c)   ((__ismask(c)&(_U|_L)) != 0)
#define rt_iscntrl(c)   ((__ismask(c)&(_C)) != 0)
#define rt_isgraph(c)   ((__ismask(c)&(_P|_U|_L|_D)) != 0)
#define rt_islower(c)   ((__ismask(c)&(_L)) != 0)
#define rt_isprint(c)   ((__ismask(c)&(_P|_U|_L|_D|_SP)) != 0)
#define rt_ispunct(c)   ((__ismask(c)&(_P)) != 0)
#define rt_isspace(c)   ((__ismask(c)&(_S)) != 0)
#define rt_isupper(c)   ((__ismask(c)&(_U)) != 0)
#define rt_isxdigit(c)  ((__ismask(c)&(_D|_X)) != 0)

#define isascii(c) (((unsigned char)(c)) <= 0x7f)
#define toascii(c) (((unsigned char)(c)) & 0x7f)

static inline unsigned char __tolower(unsigned char c)
{
    if (rt_isupper(c))
        c -= 'A'-'a';
    return c;
}

static inline unsigned char __toupper(unsigned char c)
{
    if (rt_islower(c))
        c -= 'a'-'A';
    return c;
}

int rt_tolower(int c)
{
    return __tolower(c);
}

int rt_toupper(int c)
{
    return __toupper(c);
}
/*
 * This function will get errno
 *
 * @return errno
 */
rt_err_t rt_get_errno(void)
{
    rt_thread_t tid;

    if (rt_interrupt_get_nest() != 0)
    {
        /* it's in interrupt context */
        return __rt_errno;
    }

    tid = rt_thread_self();
    if (tid == RT_NULL)
        return __rt_errno;

    return tid->error;
}
RTM_EXPORT(rt_get_errno);

/*
 * This function will set errno
 *
 * @param error the errno shall be set
 */
void rt_set_errno(rt_err_t error)
{
    rt_thread_t tid;

    if (rt_interrupt_get_nest() != 0)
    {
        /* it's in interrupt context */
        __rt_errno = error;

        return;
    }

    tid = rt_thread_self();
    if (tid == RT_NULL)
    {
        __rt_errno = error;

        return;
    }

    tid->error = error;
}
RTM_EXPORT(rt_set_errno);

/**
 * This function returns errno.
 *
 * @return the errno in the system
 */
int *_rt_errno(void)
{
    rt_thread_t tid;

    if (rt_interrupt_get_nest() != 0)
        return (int *)&__rt_errno;

    tid = rt_thread_self();
    if (tid != RT_NULL)
        return (int *) &(tid->error);

    return (int *)&__rt_errno;
}
RTM_EXPORT(_rt_errno);

/**
 * This function will set the content of memory to specified value
 *
 * @param s the address of source memory
 * @param c the value shall be set in content
 * @param count the copied length
 *
 * @return the address of source memory
 */
void *rt_memset(void *s, int c, rt_ubase_t count)
{
#ifdef RT_USING_TINY_SIZE
    char *xs = (char *)s;

    while (count--)
        *xs++ = c;

    return s;
#else
#define LBLOCKSIZE      (sizeof(rt_int32_t))
#define UNALIGNED(X)    ((rt_int32_t)X & (LBLOCKSIZE - 1))
#define TOO_SMALL(LEN)  ((LEN) < LBLOCKSIZE)

    int i;
    char *m = (char *)s;
    rt_uint32_t buffer;
    rt_uint32_t *aligned_addr;
    rt_uint32_t d = c & 0xff;  /* To avoid sign extension, copy C to an
                                unsigned variable.  */

    if (count >= 4)
    {
        while ((rt_uint32_t)m & 3)
        {
            *m++ = (char)d;
            count--;
        }
    }

    if (!TOO_SMALL(count) && !UNALIGNED(m))
    {
        /* If we get this far, we know that n is large and m is word-aligned. */
        aligned_addr = (rt_uint32_t *)m;

        /* Store D into each char sized location in BUFFER so that
         * we can set large blocks quickly.
         */
        if (LBLOCKSIZE == 4)
        {
            buffer = (d << 8) | d;
            buffer |= (buffer << 16);
        }
        else
        {
            buffer = 0;
            for (i = 0; i < LBLOCKSIZE; i++)
                buffer = (buffer << 8) | d;
        }

        while (count >= LBLOCKSIZE * 4)
        {
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            count -= 4 * LBLOCKSIZE;
        }

        while (count >= LBLOCKSIZE)
        {
            *aligned_addr++ = buffer;
            count -= LBLOCKSIZE;
        }

        /* Pick up the remainder with a bytewise loop. */
        m = (char *)aligned_addr;
    }

    while (count--)
    {
        *m++ = (char)d;
    }

    return s;

#undef LBLOCKSIZE
#undef UNALIGNED
#undef TOO_SMALL
#endif
}
RTM_EXPORT(rt_memset);

/**
 * This function will copy memory content from source address to destination
 * address.
 *
 * @param dst the address of destination memory
 * @param src  the address of source memory
 * @param count the copied length
 *
 * @return the address of destination memory
 */

void arm32_alignedcopy(void *src, void *dst, rt_ubase_t size);
void *rt_memcpy(void *dst, const void *src, rt_ubase_t count)
{
#ifdef RT_USING_TINY_SIZE
    char *tmp = (char *)dst, *s = (char *)src;
    rt_ubase_t len;

    if (tmp <= s || tmp > (s + count))
    {
        while (count--)
            *tmp++  = *s++;
    }
    else
    {
        for (len = count; len > 0; len--)
            tmp[len - 1] = s[len - 1];
    }

    return dst;
#else

#define UNALIGNED(X, Y)                                               \
                        (((rt_int32_t)X & (sizeof(rt_int32_t) - 1)) | \
                         ((rt_int32_t)Y & (sizeof(rt_int32_t) - 1)))
#define BIGBLOCKSIZE    (32)
#define LITTLEBLOCKSIZE (sizeof(rt_int32_t))
#define TOO_SMALL(LEN)  ((LEN) < BIGBLOCKSIZE)

    char *dst_ptr = (char *)dst;
    char *src_ptr = (char *)src;
    rt_int32_t *aligned_dst;
    rt_int32_t *aligned_src;
    int len = count;
    rt_uint32_t over;

    over = (rt_uint32_t)dst_ptr & 3;
    if (over && !TOO_SMALL(len) && (over == ((rt_uint32_t)src_ptr & 3)))
    {
        if (over == 2)
        {
            *((rt_uint16_t *)dst_ptr) = *((rt_uint16_t *)src_ptr);
        }
        else if (over == 1)
        {
            *dst_ptr = *src_ptr;
            *((rt_uint16_t *)(dst_ptr+1)) = *((rt_uint16_t *)(src_ptr+1));
            over = 3;
        }
        else /*must be 3*/
        {
            *dst_ptr = *src_ptr;
            over = 1;
        }

        dst_ptr += over;
        src_ptr += over;
        len     -= over;
    }
    /* If the size is small, or either SRC or DST is unaligned,
    then punt into the byte copy loop.  This should be rare. */
    if (!TOO_SMALL(len) && (!UNALIGNED(src_ptr, dst_ptr) || addr_all_cached(src_ptr, dst_ptr)))
    {
        aligned_dst = (rt_int32_t *)dst_ptr;
        aligned_src = (rt_int32_t *)src_ptr;
        if (!UNALIGNED(src_ptr, dst_ptr))
        {
            if (len >= BIGBLOCKSIZE)
            {
                arm32_alignedcopy(aligned_src, aligned_dst, ((len>>5)<<5));
                aligned_src = (rt_int32_t *)((rt_uint32_t)src_ptr +((len>>5)<<5));
                aligned_dst = (rt_int32_t *)((rt_uint32_t)dst_ptr +((len>>5)<<5));
                /* Copy 32 byte at a time if possible. */
                len -= ((len>>5)<<5);
            }
        }
        else
        {
            /* Copy 4X long words at a time if possible. */
            while (len >= BIGBLOCKSIZE)
            {
                *aligned_dst++ = *aligned_src++;
                *aligned_dst++ = *aligned_src++;
                *aligned_dst++ = *aligned_src++;
                *aligned_dst++ = *aligned_src++;
                *aligned_dst++ = *aligned_src++;
                *aligned_dst++ = *aligned_src++;
                *aligned_dst++ = *aligned_src++;
                *aligned_dst++ = *aligned_src++;
                len -= BIGBLOCKSIZE;
            }
            /* Copy one long word at a time if possible. */
            while (len >= LITTLEBLOCKSIZE)
            {
                *aligned_dst++ = *aligned_src++;
                len -= LITTLEBLOCKSIZE;
            }
        }
        /* Pick up any residual with a byte copier. */
        dst_ptr = (char *)aligned_dst;
        src_ptr = (char *)aligned_src;
    }
    while (len--)
        *dst_ptr++ = *src_ptr++;

    return dst;
#undef UNALIGNED
#undef BIGBLOCKSIZE
#undef LITTLEBLOCKSIZE
#undef TOO_SMALL
#endif
}
RTM_EXPORT(rt_memcpy);

/**
 * This function will move memory content from source address to destination
 * address.
 *
 * @param dest the address of destination memory
 * @param src  the address of source memory
 * @param n the copied length
 *
 * @return the address of destination memory
 */
void *rt_memmove(void *dest, const void *src, rt_ubase_t n)
{
    char *tmp = (char *)dest, *s = (char *)src;

    if (s < tmp && tmp < s + n)
    {
        tmp += n;
        s += n;

        while (n--)
            *(--tmp) = *(--s);
    }
    else
    {
        while (n--)
            *tmp++ = *s++;
    }

    return dest;
}
RTM_EXPORT(rt_memmove);

/**
 * This function will compare two areas of memory
 *
 * @param cs one area of memory
 * @param ct znother area of memory
 * @param count the size of the area
 *
 * @return the result
 */
rt_int32_t rt_memcmp(const void *cs, const void *ct, rt_ubase_t count)
{
    const unsigned char *su1, *su2;
    int res = 0;

    for (su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
        if ((res = *su1 - *su2) != 0)
            break;

    return res;
}
RTM_EXPORT(rt_memcmp);

/**
 * memscan - Find a character in an area of memory.
 * @addr: The memory area
 * @c: The byte to search for
 * @size: The size of the area.
 *
 * returns the address of the first occurrence of @c, or 1 byte past
 * the area if @c is not found
 */
void *rt_memscan(void *addr, int c, size_t size)
{
    unsigned char *p = addr;

    while (size)
    {
        if (*p == c)
            return (void *)p;
        p++;
        size--;
    }
      return (void *)p;
}
RTM_EXPORT(rt_memscan);

/**
 * This function will return the first occurrence of a string.
 *
 * @param s1 the source string
 * @param s2 the find string
 *
 * @return the first occurrence of a s2 in s1, or RT_NULL if no found.
 */
char *rt_strstr(const char *s1, const char *s2)
{
    int l1, l2;

    l2 = rt_strlen(s2);
    if (!l2)
        return (char *)s1;
    l1 = rt_strlen(s1);
    while (l1 >= l2)
    {
        l1--;
        if (!rt_memcmp(s1, s2, l2))
            return (char *)s1;
        s1++;
    }

    return RT_NULL;
}
RTM_EXPORT(rt_strstr);

/**
 * strnstr - Find the first substring in a length-limited string
 * @s1: The string to be searched
 * @s2: The string to search for
 * @len: the maximum number of characters to search
 */
char *rt_strnstr(const char *s1, const char *s2, size_t len)
{
    size_t l2;

    l2 = rt_strlen(s2);
    if (!l2)
        return (char *)s1;
    while (len >= l2)
    {
        len--;
        if (!rt_memcmp(s1, s2, l2))
            return (char *)s1;
        s1++;
    }
    return NULL;
}
RTM_EXPORT(rt_strnstr);

/**
 * memchr - Find a character in an area of memory.
 * @s: The memory area
 * @c: The byte to search for
 * @n: The size of the area.
 *
 * returns the address of the first occurrence of @c, or %NULL
 * if @c is not found
 */
void *rt_memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;

    while (n-- != 0)
    {
        if ((unsigned char)c == *p++)
        {
            return (void *)(p - 1);
        }
    }
    return NULL;
}
RTM_EXPORT(rt_memchr);

static void *rt_check_bytes8(const rt_uint8_t *start, rt_uint8_t value, unsigned int bytes)
{
    while (bytes)
    {
        if (*start != value)
            return (void *)start;
        start++;
        bytes--;
    }
    return NULL;
}

/**
 * memchr_inv - Find an unmatching character in an area of memory.
 * @start: The memory area
 * @c: Find a character other than c
 * @bytes: The size of the area.
 *
 * returns the address of the first character other than @c, or %NULL
 * if the whole buffer contains just @c.
 */
void *rt_memchr_inv(const void *start, int c, size_t bytes)
{
    rt_uint8_t value = c;
    rt_uint64_t value64;
    unsigned int words, prefix;

    if (bytes <= 16)
        return rt_check_bytes8(start, value, bytes);

    value64 = value;
    value64 |= value64 << 8;
    value64 |= value64 << 16;
    value64 |= value64 << 32;

    prefix = (unsigned long)start % 8;
    if (prefix)
    {
        rt_uint8_t *r;

        prefix = 8 - prefix;
        r = rt_check_bytes8(start, value, prefix);
        if (r)
            return r;
        start += prefix;
        bytes -= prefix;
    }

    words = bytes / 8;

    while (words)
    {
        if (*(rt_uint64_t *)start != value64)
            return rt_check_bytes8(start, value, 8);
        start += 8;
        words--;
    }

    return rt_check_bytes8(start, value, bytes % 8);
}
RTM_EXPORT(rt_memchr_inv);

/**
 * strncasecmp - Case insensitive, length-limited string comparison
 * @s1: One string
 * @s2: The other string
 * @len: the maximum number of characters to compare
 */
int rt_strncasecmp(const char *s1, const char *s2, size_t len)
{
    /* Yes, Virginia, it had better be unsigned */
    unsigned char c1, c2;

    if (!len)
        return 0;

    do
    {
        c1 = *s1++;
        c2 = *s2++;
        if (!c1 || !c2)
            break;
        if (c1 == c2)
            continue;
        c1 = rt_tolower(c1);
        c2 = rt_tolower(c2);
        if (c1 != c2)
            break;
    } while (--len);
    return (int)c1 - (int)c2;
}
RTM_EXPORT(rt_strncasecmp);

/**
 * This function will compare two strings while ignoring differences in case
 *
 * @param a the string to be compared
 * @param b the string to be compared
 *
 * @return the result
 */
rt_uint32_t rt_strcasecmp(const char *a, const char *b)
{
    int ca, cb;

    do
    {
        ca = *a++ & 0xff;
        cb = *b++ & 0xff;
        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';
    } while (ca == cb && ca != '\0');

    return ca - cb;
}
RTM_EXPORT(rt_strcasecmp);

char *rt_strcpy(char *dest, const char *src)
{
    char *tmp = dest;

    while ((*dest++ = *src++) != '\0')
        /* nothing */;
    return tmp;
}
RTM_EXPORT(rt_strcpy);

/**
 * This function will copy string no more than n bytes.
 *
 * @param dst the string to copy
 * @param src the string to be copied
 * @param n the maximum copied length
 *
 * @return the result
 */
char *rt_strncpy(char *dst, const char *src, rt_ubase_t n)
{
    if (n != 0)
    {
        char *d = dst;
        const char *s = src;

        do
        {
            if ((*d++ = *s++) == 0)
            {
                /* NUL pad the remaining n-1 bytes */
                while (--n != 0)
                    *d++ = 0;
                break;
            }
        } while (--n != 0);
    }

    return (dst);
}
RTM_EXPORT(rt_strncpy);

/**
 * strlcpy - Copy a C-string into a sized buffer
 * @dest: Where to copy the string to
 * @src: Where to copy the string from
 * @size: size of destination buffer
 *
 * Compatible with *BSD: the result is always a valid
 * NUL-terminated string that fits in the buffer (unless,
 * of course, the buffer size is zero). It does not pad
 * out the result like strncpy() does.
 */
size_t rt_strlcpy(char *dest, const char *src, size_t size)
{
    size_t ret = rt_strlen(src);

    if (size)
    {
        size_t len = (ret >= size) ? size - 1 : ret;
        rt_memcpy(dest, src, len);
        dest[len] = '\0';
    }
    return ret;
}

RTM_EXPORT(rt_strlcpy);

/**
 * strcat - Append one %NUL-terminated string to another
 * @dest: The string to be appended to
 * @src: The string to append to it
 */
char *rt_strcat(char *dest, const char *src)
{
    char *tmp = dest;

    while (*dest)
        dest++;
    while ((*dest++ = *src++) != '\0')
        ;
    return tmp;
}
RTM_EXPORT(rt_strcat);

/**
 * strncat - Append a length-limited, C-string to another
 * @dest: The string to be appended to
 * @src: The string to append to it
 * @count: The maximum numbers of bytes to copy
 *
 * Note that in contrast to strncpy(), strncat() ensures the result is
 * terminated.
 */
char *rt_strncat(char *dest, const char *src, size_t count)
{
    char *tmp = dest;

    if (count)
    {
        while (*dest)
            dest++;
        while ((*dest++ = *src++) != 0)
        {
            if (--count == 0)
            {
                *dest = '\0';
                break;
            }
        }
    }
    return tmp;
}
RTM_EXPORT(rt_strncat);

/**
 * strlcat - Append a length-limited, C-string to another
 * @dest: The string to be appended to
 * @src: The string to append to it
 * @count: The size of the destination buffer.
 */
size_t rt_strlcat(char *dest, const char *src, size_t count)
{
    size_t dsize = rt_strlen(dest);
    size_t len = rt_strlen(src);
    size_t res = dsize + len;

    /* This would be a bug */
    RT_ASSERT(!(dsize >= count));

    dest += dsize;
    count -= dsize;
    if (len >= count)
        len = count-1;
    rt_memcpy(dest, src, len);
    dest[len] = 0;
    return res;
}
RTM_EXPORT(rt_strlcat);

char *rt_strpbrk(const char *s1, const char *s2)
{
    const char *s;
    const char *p;

    for (s = s1 ; *s ; s++)
    {
        for (p = s2 ; *p ; p++)
        {
            if (*p == *s)
                return (char *)s;
        }
    }
    return RT_NULL;
}
RTM_EXPORT(rt_strpbrk);

char *rt_strsep(char **s1, const char *s2)
{
    char *s;
    char *p;

    if (!s1 || !s2)
        return RT_NULL;

    s = *s1;
    p = RT_NULL;
    if (s && *s)
    {
        p = rt_strpbrk(s, s2);
        if (p)
            *p++ = 0;
    }
    *s1 = p;

    return s;
}
RTM_EXPORT(rt_strsep);

int rt_atoi(const char *nptr)
{
    int ret = 0;
    int sign = 0;
    char c;

    if (nptr)
    {
        if (*nptr == '+')
        {
            nptr++;
        }
        else if (*nptr == '-')
        {
            sign = 1;
            nptr++;
        }

        while (1)
        {
            c = *(nptr++);
            if (!(c >= '0' && c <= '9'))
                break;
            ret = ret * 10 + (int)(c - '0');
        }

        if (sign)
            ret = -ret;
    }

    return ret;
}
RTM_EXPORT(rt_atoi);

/**
 * This function will compare two strings with specified maximum length
 *
 * @param cs the string to be compared
 * @param ct the string to be compared
 * @param count the maximum compare length
 *
 * @return the result
 */
rt_int32_t rt_strncmp(const char *cs, const char *ct, rt_ubase_t count)
{
    register signed char __res = 0;

    while (count)
    {
        if ((__res = *cs - *ct++) != 0 || !*cs++)
            break;
        count--;
    }

    return __res;
}
RTM_EXPORT(rt_strncmp);

/**
 * strchr - Find the first occurrence of a character in a string
 * @s: The string to be searched
 * @c: The character to search for
 */
char *rt_strchr(const char *s, int c)
{
    for (; *s != (char)c; ++s)
        if (*s == '\0')
            return NULL;
    return (char *)s;
}
RTM_EXPORT(rt_strchr);

/**
 * strrchr - Find the last occurrence of a character in a string
 * @s: The string to be searched
 * @c: The character to search for
 */
char *rt_strrchr(const char *s, int c)
{
    const char *last = NULL;

    do
    {
        if (*s == (char)c)
            last = s;
    } while (*s++);
    return (char *)last;
}
RTM_EXPORT(rt_strrchr);

/**
 * strnchr - Find a character in a length limited string
 * @s: The string to be searched
 * @count: The number of characters to be searched
 * @c: The character to search for
 */
char *rt_strnchr(const char *s, size_t count, int c)
{
    for (; count-- && *s != '\0'; ++s)
        if (*s == (char)c)
            return (char *)s;
    return NULL;
}
RTM_EXPORT(rt_strnchr);

/**
 * skip_spaces - Removes leading whitespace from @str.
 * @str: The string to be stripped.
 *
 * Returns a pointer to the first non-whitespace character in @str.
 */
char *rt_skip_spaces(const char *str)
{
    while (rt_isspace(*str))
        ++str;
    return (char *)str;
}
RTM_EXPORT(rt_skip_spaces);

/**
 * strim - Removes leading and trailing whitespace from @s.
 * @s: The string to be stripped.
 *
 * Note that the first trailing whitespace is replaced with a %NUL-terminator
 * in the given string @s. Returns a pointer to the first non-whitespace
 * character in @s.
 */
char *rt_strim(char *s)
{
    size_t size;
    char *end;

    size = rt_strlen(s);
    if (!size)
        return s;

    end = s + size - 1;
    while (end >= s && rt_isspace(*end))
        end--;
    *(end + 1) = '\0';

    return rt_skip_spaces(s);
}
RTM_EXPORT(rt_strim);

char *rt_strstrip(char *str)
{
    return rt_strim(str);
}
RTM_EXPORT(rt_strstrip);

/**
 * This function will compare two strings without specified length
 *
 * @param cs the string to be compared
 * @param ct the string to be compared
 *
 * @return the result
 */
rt_int32_t rt_strcmp(const char *cs, const char *ct)
{
    while (*cs && *cs == *ct)
        cs++, ct++;

    return (*cs - *ct);
}
RTM_EXPORT(rt_strcmp);

/**
 * The  strnlen()  function  returns the number of characters in the
 * string pointed to by s, excluding the terminating null byte ('\0'),
 * but at most maxlen.  In doing this, strnlen() looks only at the
 * first maxlen characters in the string pointed to by s and never
 * beyond s+maxlen.
 *
 * @param s the string
 * @param maxlen the max size
 * @return the length of string
 */
rt_size_t rt_strnlen(const char *s, rt_ubase_t maxlen)
{
    const char *sc;

    for (sc = s; *sc != '\0' && (rt_ubase_t)(sc - s) < maxlen; ++sc) /* nothing */
        ;

    return sc - s;
}
RTM_EXPORT(rt_strnlen);

/**
 * This function will return the length of a string, which terminate will
 * null character.
 *
 * @param s the string
 *
 * @return the length of string
 */
rt_size_t rt_strlen(const char *s)
{
    const char *sc;

    for (sc = s; *sc != '\0'; ++sc) /* nothing */
        ;

    return sc - s;
}
RTM_EXPORT(rt_strlen);

/**
 * strspn - Calculate the length of the initial substring of @s which only contain letters in @accept
 * @s: The string to be searched
 * @accept: The string to search for
 */
size_t rt_strspn(const char *s, const char *accept)
{
    const char *p;
    const char *a;
    size_t count = 0;

    for (p = s; *p != '\0'; ++p)
    {
        for (a = accept; *a != '\0'; ++a)
        {
            if (*p == *a)
                break;
        }
        if (*a == '\0')
            return count;
        ++count;
    }
    return count;
}

RTM_EXPORT(rt_strspn);

/**
 * strcspn - Calculate the length of the initial substring of @s which does not contain letters in @reject
 * @s: The string to be searched
 * @reject: The string to avoid
 */
size_t rt_strcspn(const char *s, const char *reject)
{
    const char *p;
    const char *r;
    size_t count = 0;

    for (p = s; *p != '\0'; ++p)
    {
        for (r = reject; *r != '\0'; ++r)
        {
            if (*p == *r)
                return count;
        }
        ++count;
    }
    return count;
}
RTM_EXPORT(rt_strcspn);

#ifdef RT_USING_HEAP
/**
 * This function will duplicate a string.
 *
 * @param s the string to be duplicated
 *
 * @return the duplicated string pointer
 */
char *rt_strdup(const char *s)
{
    rt_size_t len = rt_strlen(s) + 1;
    char *tmp = (char *)rt_malloc(len);

    if (!tmp)
        return RT_NULL;

    rt_memcpy(tmp, s, len);

    return tmp;
}
RTM_EXPORT(rt_strdup);
#if defined(__CC_ARM) || defined(__CLANG_ARM)
char *strdup(const char *s) __attribute__((alias("rt_strdup")));
#endif
#endif

/**
 * This function will show the version of rt-thread rtos
 */
#include <version.h>

void rt_show_version(void)
{
    rt_kprintf("\n \\ | /\n");
    rt_kprintf("- RT -     Thread Operating System\n");
    rt_kprintf(" / | \\     %d.%d.%d build %s\n",
               RT_VERSION, RT_SUBVERSION, RT_REVISION, __DATE__);
    rt_kprintf(" 2006 - 2019 Copyright by rt-thread team\n");
    rt_kprintf(" SDK %s-%s Copyright by www.fullhan.com\n", CURRENT_VERSION, CURRENT_RELEASE);
}
RTM_EXPORT(rt_show_version);

/* private function */
#define isdigit(c)  ((unsigned int)((c) - '0') < 10)

rt_inline rt_int32_t divide_64(long long *n, rt_int32_t base)
{
    rt_int32_t res;

    /* optimized for processor which does not support divide instructions. */
    if (base == 10)
    {
        res = (*n) % 10U;
        *n = (*n) / 10U;
    }
    else
    {
        res = (*n) % 16U;
        *n = (*n) / 16U;
    }

    return res;
}

rt_inline rt_int32_t divide(rt_int32_t *n, rt_int32_t base)
{
    rt_int32_t res;

    /* optimized for processor which does not support divide instructions. */
    if (base == 10)
    {
        res = ((rt_uint32_t) *n) % 10U;
        *n = ((rt_uint32_t) *n) / 10U;
    }
    else
    {
        res = ((rt_uint32_t) *n) % 16U;
        *n = ((rt_uint32_t) *n) / 16U;
    }

    return res;
}

rt_inline int skip_atoi(const char **s)
{
    register int i = 0;
    while (isdigit(**s))
        i = i * 10 + *((*s)++) - '0';

    return i;
}

/**
 * simple_strtoul - convert a string to an unsigned long
 * @cp: The start of the string
 * @endp: A pointer to the end of the parsed string will be placed here
 * @base: The number base to use
 */
unsigned long rt_simple_strtoul(const char *cp, char **endp, unsigned int base)
{
    unsigned long result = 0, value;

    if (!base)
    {
        base = 10;
        if (*cp == '0')
        {
            base = 8;
            cp++;
            if ((rt_toupper(*cp) == 'X') && rt_isxdigit(cp[1]))
            {
                cp++;
                base = 16;
            }
        }
    } else if (base == 16)
    {
        if (cp[0] == '0' && rt_toupper(cp[1]) == 'X')
            cp += 2;
    }
    while (rt_isxdigit(*cp) &&
           (value = isdigit(*cp) ? *cp-'0' : rt_toupper(*cp)-'A'+10) < base)
    {
        result = result*base + value;
        cp++;
    }
    if (endp)
        *endp = (char *)cp;
    return result;
}

/**
 * simple_strtol - convert a string to a signed long
 * @cp: The start of the string
 * @endp: A pointer to the end of the parsed string will be placed here
 * @base: The number base to use
 */
long rt_simple_strtol(const char *cp, char **endp, unsigned int base)
{
    if (*cp == '-')
        return -rt_simple_strtoul(cp+1, endp, base);
    return rt_simple_strtoul(cp, endp, base);
}

/**
 * simple_strtoull - convert a string to an unsigned long long
 * @cp: The start of the string
 * @endp: A pointer to the end of the parsed string will be placed here
 * @base: The number base to use
 */
unsigned long long rt_simple_strtoull(const char *cp, char **endp, unsigned int base)
{
    unsigned long long result = 0, value;

    if (*cp == '0')
    {
        cp++;
        if ((rt_toupper(*cp) == 'X') && rt_isxdigit (cp[1]))
        {
            base = 16;
            cp++;
        }
        if (!base)
        {
            base = 8;
        }
    }
    if (!base)
    {
        base = 10;
    }
    while (rt_isxdigit (*cp) && (value = isdigit (*cp)
                ? *cp - '0'
                : (rt_islower(*cp) ? rt_toupper(*cp) : *cp) - 'A' + 10) < base)
    {
        result = result * base + value;
        cp++;
    }
    if (endp)
        *endp = (char *) cp;
    return result;
}

/**
 * simple_strtoll - convert a string to a signed long long
 * @cp: The start of the string
 * @endp: A pointer to the end of the parsed string will be placed here
 * @base: The number base to use
 */
long long rt_simple_strtoll(const char *cp, char **endp, unsigned int base)
{
    if (*cp == '-')
        return -rt_simple_strtoull(cp+1, endp, base);
    return rt_simple_strtoull(cp, endp, base);
}

#define ZEROPAD     (1 << 0)    /* pad with zero */
#define SIGN        (1 << 1)    /* unsigned/signed long */
#define PLUS        (1 << 2)    /* show plus */
#define SPACE       (1 << 3)    /* space if plus */
#define LEFT        (1 << 4)    /* left justified */
#define SPECIAL     (1 << 5)    /* 0x */
#define LARGE       (1 << 6)    /* use 'ABCDEF' instead of 'abcdef' */

static char *print_number_ll(char *buf,
                          char *end,
                          long long  num,
                          int   base,
                          int   s,
                          int   precision,
                          int   type)
{
    char c, sign;
#ifdef RT_PRINTF_LONGLONG
    char tmp[32];
#else
    char tmp[16];
#endif
    int precision_bak = precision;
    const char *digits;
    static const char small_digits[] = "0123456789abcdef";
    static const char large_digits[] = "0123456789ABCDEF";

    register int i;
    register int size;

    size = s;

    digits = (type & LARGE) ? large_digits : small_digits;
    if (type & LEFT)
        type &= ~ZEROPAD;

    c = (type & ZEROPAD) ? '0' : ' ';

    /* get sign */
    sign = 0;
    if (type & SIGN)
    {
        if (num < 0)
        {
            sign = '-';
            num = -num;
        }
        else if (type & PLUS)
            sign = '+';
        else if (type & SPACE)
            sign = ' ';
    }

#ifdef RT_PRINTF_SPECIAL
    if (type & SPECIAL)
    {
        if (base == 16)
            size -= 2;
        else if (base == 8)
            size--;
    }
#endif

    i = 0;
    if (num == 0)
        tmp[i++] = '0';
    else
    {
        while (num != 0)
            tmp[i++] = digits[divide_64(&num, base)];
    }

#ifdef RT_PRINTF_PRECISION
    if (i > precision)
        precision = i;
    size -= precision;
#else
    size -= i;
#endif

    if (!(type & (ZEROPAD | LEFT)))
    {
        if ((sign) && (size > 0))
            size--;

        while (size-- > 0)
        {
            if (buf <= end)
                *buf = ' ';
            ++buf;
        }
    }

    if (sign)
    {
        if (buf <= end)
        {
            *buf = sign;
            --size;
        }
        ++buf;
    }

#ifdef RT_PRINTF_SPECIAL
    if (type & SPECIAL)
    {
        if (base == 8)
        {
            if (buf <= end)
                *buf = '0';
            ++buf;
        }
        else if (base == 16)
        {
            if (buf <= end)
                *buf = '0';
            ++buf;
            if (buf <= end)
            {
                *buf = type & LARGE ? 'X' : 'x';
            }
            ++buf;
        }
    }
#endif

    /* no align to the left */
    if (!(type & LEFT))
    {
        while (size-- > 0)
        {
            if (buf <= end)
                *buf = c;
            ++buf;
        }
    }

#ifdef RT_PRINTF_PRECISION
    while (i < precision--)
    {
        if (buf <= end)
            *buf = '0';
        ++buf;
    }
#endif

    /* put number in the temporary buffer */
    while (i-- > 0 && (precision_bak != 0))
    {
        if (buf <= end)
            *buf = tmp[i];
        ++buf;
    }

    while (size-- > 0)
    {
        if (buf <= end)
            *buf = ' ';
        ++buf;
    }

    return buf;
}
static char *print_number(char *buf,
                          char *end,
                          long  num,
                          int   base,
                          int   s,
                          int   precision,
                          int   type)
{
    char c, sign;
#ifdef RT_PRINTF_LONGLONG
    char tmp[32];
#else
    char tmp[16];
#endif
    int precision_bak = precision;
    const char *digits;
    static const char small_digits[] = "0123456789abcdef";
    static const char large_digits[] = "0123456789ABCDEF";
    register int i;
    register int size;

    size = s;

    digits = (type & LARGE) ? large_digits : small_digits;
    if (type & LEFT)
        type &= ~ZEROPAD;

    c = (type & ZEROPAD) ? '0' : ' ';

    /* get sign */
    sign = 0;
    if (type & SIGN)
    {
        if (num < 0)
        {
            sign = '-';
            num = -num;
        }
        else if (type & PLUS)
            sign = '+';
        else if (type & SPACE)
            sign = ' ';
    }

#ifdef RT_PRINTF_SPECIAL
    if (type & SPECIAL)
    {
        if (base == 16)
            size -= 2;
        else if (base == 8)
            size--;
    }
#endif

    i = 0;
    if (num == 0)
        tmp[i++] = '0';
    else
    {
        while (num != 0)
            tmp[i++] = digits[divide(&num, base)];
    }

#ifdef RT_PRINTF_PRECISION
    if (i > precision)
        precision = i;
    size -= precision;
#else
    size -= i;
#endif

    if (!(type & (ZEROPAD | LEFT)))
    {
        if ((sign) && (size > 0))
            size--;

        while (size-- > 0)
        {
            if (buf < end)
                *buf = ' ';
            ++buf;
        }
    }

    if (sign)
    {
        if (buf < end)
        {
            *buf = sign;
        }
        --size;
        ++buf;
    }

#ifdef RT_PRINTF_SPECIAL
    if (type & SPECIAL)
    {
        if (base == 8)
        {
            if (buf < end)
                *buf = '0';
            ++buf;
        }
        else if (base == 16)
        {
            if (buf < end)
                *buf = '0';
            ++buf;
            if (buf < end)
            {
                *buf = type & LARGE ? 'X' : 'x';
            }
            ++buf;
        }
    }
#endif

    /* no align to the left */
    if (!(type & LEFT))
    {
        while (size-- > 0)
        {
            if (buf < end)
                *buf = c;
            ++buf;
        }
    }

#ifdef RT_PRINTF_PRECISION
    while (i < precision--)
    {
        if (buf < end)
            *buf = '0';
        ++buf;
    }
#endif

    /* put number in the temporary buffer */
    while (i-- > 0 && (precision_bak != 0))
    {
        if (buf < end)
            *buf = tmp[i];
        ++buf;
    }

    while (size-- > 0)
    {
        if (buf < end)
            *buf = ' ';
        ++buf;
    }

    return buf;
}

rt_int32_t rt_vsnprintf(char       *buf,
                        rt_size_t   size,
                        const char *fmt,
                        va_list     args)
{
#ifdef RT_PRINTF_LONGLONG
    unsigned long long num;
#else
    rt_uint32_t num;
#endif
    int i, len;
    char *str, *end, c;
    const char *s;

    rt_uint8_t base;            /* the base of number */
    rt_uint8_t flags;           /* flags to print number */
    rt_uint8_t qualifier;       /* 'h', 'l', or 'L' for integer fields */
    rt_int32_t field_width;     /* width of output field */

    int precision;      /* min. # of digits for integers and max for a string */

    str = buf;
    end = buf + size;

    /* Make sure end is always >= buf */
    if (end < buf)
    {
        end  = ((char *) -1);
        size = end - buf;
    }

    for (; *fmt ; ++fmt)
    {
        if (*fmt != '%')
        {
            if (str < end)
                *str = *fmt;
            ++str;
            continue;
        }

        /* process flags */
        flags = 0;

        while (1)
        {
            /* skips the first '%' also */
            ++fmt;
            if (*fmt == '-')
                flags |= LEFT;
            else if (*fmt == '+')
                flags |= PLUS;
            else if (*fmt == ' ')
                flags |= SPACE;
            else if (*fmt == '#')
                flags |= SPECIAL;
            else if (*fmt == '0')
                flags |= ZEROPAD;
            else
                break;
        }

        /* get field width */
        field_width = -1;
        if (isdigit(*fmt))
            field_width = skip_atoi(&fmt);
        else if (*fmt == '*')
        {
            ++fmt;
            /* it's the next argument */
            field_width = va_arg(args, int);
            if (field_width < 0)
            {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

#ifdef RT_PRINTF_PRECISION
        /* get the precision */
        precision = -1;
        if (*fmt == '.')
        {
            ++fmt;
            if (isdigit(*fmt))
                precision = skip_atoi(&fmt);
            else if (*fmt == '*')
            {
                ++fmt;
                /* it's the next argument */
                precision = va_arg(args, int);
            }
            if (precision < 0)
                precision = 0;
        }
#endif
        /* get the conversion qualifier */
        qualifier = 0;
#ifdef RT_PRINTF_LONGLONG
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L')
#else
        if (*fmt == 'h' || *fmt == 'l')
#endif
        {
            qualifier = *fmt;
            ++fmt;
#ifdef RT_PRINTF_LONGLONG
            if (qualifier == 'l' && *fmt == 'l')
            {
                qualifier = 'L';
                ++fmt;
            }
#endif
        }

        /* the default base */
        base = 10;

        switch (*fmt)
        {
        case 'c':
            if (!(flags & LEFT))
            {
                while (--field_width > 0)
                {
                    if (str < end)
                        *str = ' ';
                    ++str;
                }
            }

            /* get character */
            c = (rt_uint8_t)va_arg(args, int);
            if (str < end)
                *str = c;
            ++str;

            /* put width */
            while (--field_width > 0)
            {
                if (str < end)
                    *str = ' ';
                ++str;
            }
            continue;

        case 's':
            s = va_arg(args, char *);
            if (!s)
                s = "(NULL)";

            len = rt_strlen(s);
#ifdef RT_PRINTF_PRECISION
            if (precision > 0 && len > precision)
                len = precision;
#endif

            if (!(flags & LEFT))
            {
                while (len < field_width--)
                {
                    if (str < end)
                        *str = ' ';
                    ++str;
                }
            }

            for (i = 0; i < len; ++i)
            {
                if (str < end)
                    *str = *s;
                ++str;
                ++s;
            }

            while (len < field_width--)
            {
                if (str < end)
                    *str = ' ';
                ++str;
            }
            continue;

        case 'p':
            if (field_width == -1)
            {
                field_width = sizeof(void *) << 1;
                flags |= ZEROPAD;
            }
            str = print_number(str, end,
                               (long)va_arg(args, void *),
                               16, field_width, precision, flags);
            continue;

        case '%':
            if (str < end)
                *str = '%';
            ++str;
            continue;

        /* integer number formats - set up the flags and "break" */
        case 'o':
            base = 8;
            break;

        case 'X':
            flags |= LARGE;
        case 'x':
            base = 16;
            break;

        case 'd':
        case 'i':
            flags |= SIGN;
        case 'u':
            break;

        default:
            if (str < end)
                *str = '%';
            ++str;

            if (*fmt)
            {
                if (str < end)
                    *str = *fmt;
                ++str;
            }
            else
            {
                --fmt;
            }
            continue;
        }

#ifdef RT_PRINTF_LONGLONG
        if (qualifier == 'L')
            num = va_arg(args, long long);
        else if (qualifier == 'l')
#else
        if (qualifier == 'l')
#endif
        {
            num = va_arg(args, rt_uint32_t);
            if (flags & SIGN)
                num = (rt_int32_t)num;
        }
        else if (qualifier == 'h')
        {
            num = (rt_uint16_t)va_arg(args, rt_int32_t);
            if (flags & SIGN)
                num = (rt_int16_t)num;
        }
        else
        {
            num = va_arg(args, rt_uint32_t);
            if (flags & SIGN)
                num = (rt_int32_t)num;
        }
        if (qualifier == 'L')
            str = print_number_ll(str, end, num, base, field_width, precision, flags);
        else
            str = print_number(str, end, (long)num, base, field_width, precision, flags);
    }

    if (size > 0)
    {
        if (str < end)
            *str = '\0';
        else
        {
            end[-1] = '\0';
        }
    }

    /* the trailing null byte doesn't count towards the total
    * ++str;
    */
    return str - buf;
}
RTM_EXPORT(rt_vsnprintf);

/**
 * vscnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * The return value is the number of characters which have been written into
 * the @buf not including the trailing '\0'. If @size is == 0 the function
 * returns 0.
 *
 * If you're not already dealing with a va_list consider using scnprintf().
 *
 * See the vsnprintf() documentation for format string extensions over C99.
 */
int rt_vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    int i;

    i = rt_vsnprintf(buf, size, fmt, args);

    if (i < size)
        return i;
    if (size != 0)
        return size - 1;
    return 0;
}
RTM_EXPORT(rt_vscnprintf);

/**
 * This function will fill a formatted string to buffer
 *
 * @param buf the buffer to save formatted string
 * @param size the size of buffer
 * @param fmt the format
 */
rt_int32_t rt_snprintf(char *buf, rt_size_t size, const char *fmt, ...)
{
    rt_int32_t n;
    va_list args;

    va_start(args, fmt);
    n = rt_vsnprintf(buf, size, fmt, args);
    va_end(args);

    return n;
}
RTM_EXPORT(rt_snprintf);

/**
 * This function will fill a formatted string to buffer
 *
 * @param buf the buffer to save formatted string
 * @param arg_ptr the arg_ptr
 * @param format the format
 */
rt_int32_t rt_vsprintf(char *buf, const char *format, va_list arg_ptr)
{
    return rt_vsnprintf(buf, (rt_size_t) -1, format, arg_ptr);
}
RTM_EXPORT(rt_vsprintf);

/**
 * vsscanf - Unformat a buffer into a list of arguments
 * @buf:    input buffer
 * @fmt:    format of buffer
 * @args:   arguments
 */
#define INT_MAX       2147483647    /* maximum (signed) int value */
int rt_vsscanf(const char *buf, const char *fmt, va_list args)
{
    const char *str = buf;
    char *next;
    int num = 0;
    int qualifier;
    int base;
    int field_width = -1;
    int is_sign = 0;

    while (*fmt && *str)
    {
        /* skip any white space in format */
        /* white space in format matchs any amount of
         * white space, including none, in the input.
         */
        if (rt_isspace(*fmt))
        {
            while (rt_isspace(*fmt))
                ++fmt;
            while (rt_isspace(*str))
                ++str;
        }

        /* anything that is not a conversion must match exactly */
        if (*fmt != '%' && *fmt)
        {
            if (*fmt++ != *str++)
                break;
            continue;
        }

        if (!*fmt)
            break;
        ++fmt;

        /* skip this conversion.
         * advance both strings to next white space
         */
        if (*fmt == '*')
        {
            while (!rt_isspace(*fmt) && *fmt)
                fmt++;
            while (!rt_isspace(*str) && *str)
                str++;
            continue;
        }

        /* get field width */
        if (isdigit(*fmt))
            field_width = skip_atoi(&fmt);

        /* get conversion qualifier */
        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' || *fmt == 'Z')
        {
            qualifier = *fmt;
            fmt++;
        }
        base = 10;
        is_sign = 0;

        if (!*fmt || !*str)
            break;

        switch (*fmt++)
        {
        case 'c':
        {
            char *s = (char *) va_arg(args, char*);

            if (field_width == -1)
                field_width = 1;
            do
            {
                *s++ = *str++;
            } while (field_width-- > 0 && *str);
            num++;
        }
        continue;
        case 's':
        {
            char *s = (char *) va_arg(args, char *);

            if (field_width == -1)
                field_width = INT_MAX;
            /* first, skip leading white space in buffer */
            while (rt_isspace(*str))
                str++;

            /* now copy until next white space */
            while (*str && !rt_isspace(*str) && field_width--)
            {
                *s++ = *str++;
            }
            *s = '\0';
            num++;
        }
        continue;
        case 'n':
            /* return number of characters read so far */
        {
            int *i = (int *)va_arg(args, int*);
            *i = str - buf;
        }
        continue;
        case 'o':
            base = 8;
            break;
        case 'x':
        case 'X':
            base = 16;
            break;
        case 'd':
        case 'i':
            is_sign = 1;
        case 'u':
            break;
        case '%':
            /* looking for '%' in str */
            if (*str++ != '%')
                return num;
            continue;
        default:
            /* invalid format; stop here */
            return num;
        }

        /* have some sort of integer conversion.
         * first, skip white space in buffer.
         */
        while (rt_isspace(*str))
            str++;

        if (!*str || !isdigit(*str))
            break;

        switch (qualifier)
        {
        case 'h':
            if (is_sign)
            {
                short *s = (short *) va_arg(args,  short *);
                *s = (short) rt_simple_strtol(str, &next, base);
            }
            else
            {
                unsigned short *s = (unsigned short *) va_arg(args, unsigned short *);
                *s = (unsigned short) rt_simple_strtoul(str, &next, base);
            }
            break;
        case 'l':
            if (is_sign)
            {
                long *l = (long *) va_arg(args, long *);
                *l = rt_simple_strtol(str, &next, base);
            }
            else
            {
                unsigned long *l = (unsigned long *) va_arg(args, unsigned long *);
                *l = rt_simple_strtoul(str, &next, base);
            }
            break;
        case 'L':
            if (is_sign)
            {
                long long *l = (long long *) va_arg(args, long long *);
                *l = rt_simple_strtoll(str, &next, base);
            }
            else
            {
                unsigned long long *l = (unsigned long long *) va_arg(args, unsigned long long *);
                *l = rt_simple_strtoull(str, &next, base);
            }
            break;
        case 'Z':
        {
            unsigned long *s = (unsigned long *) va_arg(args, unsigned long *);
            *s = (unsigned long) rt_simple_strtoul(str, &next, base);
        }
        break;
        default:
            if (is_sign)
            {
                int *i = (int *) va_arg(args, int *);
                *i = (int) rt_simple_strtol(str, &next, base);
            }
            else
            {
                unsigned int *i = (unsigned int *) va_arg(args, unsigned int *);
                *i = (unsigned int) rt_simple_strtoul(str, &next, base);
            }
            break;
        }
        num++;

        if (!next)
            break;
        str = next;
    }
    return num;
}
RTM_EXPORT(rt_vsscanf);

/**
 * This function will fill a formatted string to buffer
 *
 * @param buf the buffer to save formatted string
 * @param format the format
 */
rt_int32_t rt_sprintf(char *buf, const char *format, ...)
{
    rt_int32_t n;
    va_list arg_ptr;

    va_start(arg_ptr, format);
    n = rt_vsprintf(buf, format, arg_ptr);
    va_end(arg_ptr);

    return n;
}
RTM_EXPORT(rt_sprintf);

/**
 *  memparse - parse a string with mem suffixes into a number
 *  @ptr: Where parse begins
 *  @retptr: (output) Optional pointer to next char after parse completes
 *
 *  Parses a string into a number.  The number stored at @ptr is
 *  potentially suffixed with K, M, G, T, P, E.
 */

unsigned long long rt_memparse(const char *ptr, char **retptr)
{
    char *endptr;   /* local pointer to end of parsed string */

    unsigned long long ret = rt_simple_strtoull(ptr, &endptr, 0);

    switch (*endptr)
    {
    case 'E':
    case 'e':
        ret <<= 10;
    case 'P':
    case 'p':
        ret <<= 10;
    case 'T':
    case 't':
        ret <<= 10;
    case 'G':
    case 'g':
        ret <<= 10;
    case 'M':
    case 'm':
        ret <<= 10;
    case 'K':
    case 'k':
        ret <<= 10;
        endptr++;
    default:
        break;
    }

    if (retptr)
        *retptr = endptr;

    return ret;
}
RTM_EXPORT(rt_memparse);

#ifdef RT_USING_CONSOLE

#ifdef RT_USING_DEVICE
/**
 * This function returns the device using in console.
 *
 * @return the device using in console or RT_NULL
 */
rt_device_t rt_console_get_device(void)
{
    return _console_device;
}
RTM_EXPORT(rt_console_get_device);

/**
 * This function will set a device as console device.
 * After set a device to console, all output of rt_kprintf will be
 * redirected to this new device.
 *
 * @param name the name of new console device
 *
 * @return the old console device handler
 */
rt_device_t rt_console_set_device(const char *name)
{
    rt_device_t new_device, old_device;

    /* save old device */
    old_device = _console_device;

    /* find new console device */
    new_device = rt_device_find(name);

    /* check whether it's a same device */
    if (new_device == old_device)
        return RT_NULL;

    if (new_device != RT_NULL)
    {
        if (_console_device != RT_NULL)
        {
            /* close old console device */
            rt_device_close(_console_device);
        }

        /* set new console device */
        rt_device_open(new_device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_STREAM);
        _console_device = new_device;
    }

    return old_device;
}
RTM_EXPORT(rt_console_set_device);
#endif

RT_WEAK void rt_hw_console_output(const char *str)
{
    /* empty console output */
}
RTM_EXPORT(rt_hw_console_output);

/* 控制台输出分流钩子，见 rtthread.h 的声明 */
static void (*_console_output_hook)(const char *str);

void rt_console_set_output_hook(void (*hook)(const char *str))
{
    _console_output_hook = hook;
}
RTM_EXPORT(rt_console_set_output_hook);

/**
 * This function will put string to the console.
 *
 * @param str the string output to the console.
 */
void rt_kputs(const char *str)
{
    if (!str)
        return;

    if (_console_output_hook != RT_NULL)
        _console_output_hook(str);

#ifdef RT_USING_DEVICE
    if (_console_device == RT_NULL)
    {
        rt_hw_console_output(str);
    }
    else
    {
        rt_uint16_t old_flag = _console_device->open_flag;

        _console_device->open_flag |= RT_DEVICE_FLAG_STREAM;
        rt_device_write(_console_device, 0, str, rt_strlen(str));
        _console_device->open_flag = old_flag;
    }
#else
    rt_hw_console_output(str);
#endif
}

/**
 * This function will print a formatted string on system console
 *
 * @param fmt the format
*/

void rt_kprintf(const char *fmt, ...)
{
    va_list args;
    rt_size_t length;
    char rt_log_buf[RT_CONSOLEBUF_SIZE+4];

    va_start(args, fmt);
    /* the return value of vsnprintf is the number of bytes that would be
     * written to buffer had if the size of the buffer been sufficiently
     * large excluding the terminating null byte. If the output string
     * would be larger than the rt_log_buf, we have to adjust the output
     * length. */
    length = rt_vsnprintf(rt_log_buf, RT_CONSOLEBUF_SIZE, fmt, args);
    if (length > RT_CONSOLEBUF_SIZE)
        length = RT_CONSOLEBUF_SIZE;
    rt_log_buf[RT_CONSOLEBUF_SIZE] = 0; /*ensure NULL terminated*/

    if (_console_output_hook != RT_NULL)
        _console_output_hook(rt_log_buf);

#ifdef RT_USING_DEVICE
    if (_console_device == RT_NULL)
    {
        rt_hw_console_output(rt_log_buf);
    }
    else
    {
        rt_uint16_t old_flag = _console_device->open_flag;

        if (old_flag & RT_DEVICE_FLAG_STREAM)
            rt_device_write(_console_device, 0, rt_log_buf, length);
        else
        {
            _console_device->open_flag |= RT_DEVICE_FLAG_STREAM;
            rt_device_write(_console_device, 0, rt_log_buf, length);
            _console_device->open_flag = old_flag;
        }
    }
#else
    rt_hw_console_output(rt_log_buf);
#endif
    va_end(args);
}
RTM_EXPORT(rt_kprintf);
#endif

#ifdef RT_USING_HEAP
/**
 * This function allocates a memory block, which address is aligned to the
 * specified alignment size.
 *
 * @param size the allocated memory block size
 * @param align the alignment size
 *
 * @return the allocated memory block on successful, otherwise returns RT_NULL
 */
void *rt_malloc_align(rt_size_t size, rt_size_t align)
{
    void *align_ptr;
    void *ptr;
    rt_size_t align_size;

    /* align the alignment size to 4 byte */
    align = ((align + 0x03) & ~0x03);

    /* get total aligned size */
    align_size = ((size + 0x03) & ~0x03) + align;
    /* allocate memory block from heap */
    ptr = rt_malloc(align_size);
    if (ptr != RT_NULL)
    {
        /* the allocated memory block is aligned */
        if (((rt_uint32_t)ptr & (align - 1)) == 0)
        {
            align_ptr = (void *)((rt_uint32_t)ptr + align);
        }
        else
        {
            align_ptr = (void *)(((rt_uint32_t)ptr + (align - 1)) & ~(align - 1));
        }

        /* set the pointer before alignment pointer to the real pointer */
        *((rt_uint32_t *)((rt_uint32_t)align_ptr - sizeof(void *))) = (rt_uint32_t)ptr;

        ptr = align_ptr;
    }

    return ptr;
}
RTM_EXPORT(rt_malloc_align);

/**
 * This function release the memory block, which is allocated by
 * rt_malloc_align function and address is aligned.
 *
 * @param ptr the memory block pointer
 */
void rt_free_align(void *ptr)
{
    void *real_ptr;

    real_ptr = (void *) *(rt_uint32_t *)((rt_uint32_t)ptr - sizeof(void *));
    rt_free(real_ptr);
}
RTM_EXPORT(rt_free_align);
#endif

#ifndef RT_USING_CPU_FFS
const rt_uint8_t __lowest_bit_bitmap[] = {
    /* 00 */ 0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 10 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 20 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 30 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 40 */ 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 50 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 60 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 70 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 80 */ 7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 90 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* A0 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* B0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* C0 */ 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* D0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* E0 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* F0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};

/**
 * This function finds the first bit set (beginning with the least significant bit)
 * in value and return the index of that bit.
 *
 * Bits are numbered starting at 1 (the least significant bit).  A return value of
 * zero from any of these functions means that the argument was zero.
 *
 * @return return the index of the first bit set. If value is 0, then this function
 * shall return 0.
 */
int __rt_ffs(int value)
{
    if (value == 0)
        return 0;

    if (value & 0xff)
        return __lowest_bit_bitmap[value & 0xff] + 1;

    if (value & 0xff00)
        return __lowest_bit_bitmap[(value & 0xff00) >> 8] + 9;

    if (value & 0xff0000)
        return __lowest_bit_bitmap[(value & 0xff0000) >> 16] + 17;

    return __lowest_bit_bitmap[(value & 0xff000000) >> 24] + 25;
}
#endif

#ifdef RT_DEBUG
/* RT_ASSERT(EX)'s hook */
void (*rt_assert_hook)(const char *ex, const char *func, rt_size_t line);
/**
 * This function will set a hook function to RT_ASSERT(EX). It will run when the expression is false.
 *
 * @param hook the hook function
 */
void rt_assert_set_hook(void (*hook)(const char *ex, const char *func, rt_size_t line))
{
    rt_assert_hook = hook;
}

/**
 * The RT_ASSERT function.
 *
 * @param ex the assertion condition string
 * @param func the function name when assertion.
 * @param line the file line number when assertion.
 */
extern int trigger_uart_output_nolock(void);
void rt_assert_handler(const char *ex_string, const char *func, rt_size_t line)
{
    rt_base_t level;
    volatile char dummy = 0;

    if (rt_assert_hook == RT_NULL)
    {
#ifdef RT_USING_MODULE
        if (dlmodule_self())
        {
            /* close assertion module */
            dlmodule_exit(-1);
        }
        else
#endif
        {
            show_func_callchain();
            rt_kprintf("(%s) assertion failed at function:%s, line number:%d\n", ex_string, func, line);

            level = rt_hw_interrupt_disable();
            while (trigger_uart_output_nolock() != 0)
                ;
            rt_hw_interrupt_enable(level);

            while (dummy == 0)
               ;
        }
    }
    else
    {
        rt_assert_hook(ex_string, func, line);
    }
}
RTM_EXPORT(rt_assert_handler);
#endif /* RT_DEBUG */

#if !defined(RT_USING_NEWLIB) && defined(RT_USING_MINILIBC) && defined(__GNUC__)
#include <sys/types.h>
void *memcpy(void *dest, const void *src, size_t n) __attribute__((weak, alias("rt_memcpy")));
void *memset(void *s, int c, size_t n) __attribute__((weak, alias("rt_memset")));
void *memmove(void *dest, const void *src, size_t n) __attribute__((weak, alias("rt_memmove")));
int   memcmp(const void *s1, const void *s2, size_t n) __attribute__((weak, alias("rt_memcmp")));

size_t strlen(const char *s) __attribute__((weak, alias("rt_strlen")));
char *strstr(const char *s1, const char *s2) __attribute__((weak, alias("rt_strstr")));
int strcasecmp(const char *a, const char *b) __attribute__((weak, alias("rt_strcasecmp")));
char *strncpy(char *dest, const char *src, size_t n) __attribute__((weak, alias("rt_strncpy")));
int strncmp(const char *cs, const char *ct, size_t count) __attribute__((weak, alias("rt_strncmp")));
#ifdef RT_USING_HEAP
char *strdup(const char *s) __attribute__((weak, alias("rt_strdup")));
#endif

int sprintf(char *buf, const char *format, ...) __attribute__((weak, alias("rt_sprintf")));
int snprintf(char *buf, rt_size_t size, const char *fmt, ...) __attribute__((weak, alias("rt_snprintf")));
int vsprintf(char *buf, const char *format, va_list arg_ptr) __attribute__((weak, alias("rt_vsprintf")));

#endif

/**@}*/
