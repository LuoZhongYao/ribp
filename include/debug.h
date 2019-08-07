/*************************************************
 * Anthor  : LuoZhongYao@gmail.com
 * Modified: 2019/08/06
 ************************************************/
#ifndef __DEBUG_H__
#define __DEBUG_H__

#define ENABLDE_DEBUG

#ifdef ENABLDE_DEBUG
# include <stdio.h>
# define log_print printf
#else
# define log_print(...)
#endif

#define BLOGD(fmt, ...) log_print(fmt, ##__VA_ARGS__)
#define BLOGI(fmt, ...) log_print(fmt, ##__VA_ARGS__)
#define BLOGV(fmt, ...) log_print(fmt, ##__VA_ARGS__)
#define BLOGW(fmt, ...) log_print(fmt, ##__VA_ARGS__)
#define BLOGE(fmt, ...) log_print(fmt, ##__VA_ARGS__)

#endif /* __DEBUG_H__*/

