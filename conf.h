#ifndef CONF_H_
#define CONF_H_

#include <stdio.h>

#define CONFIG_SM_IPT_NUM_BLOCKS_64 1
#define CONFIG_SM_IPT_CACHE_LINE_BYTES 32

#ifdef MASTER

#define IS_MASTER 1
#define IS_SLAVE 0

#else

#define IS_MASTER 0
#define IS_SLAVE 1

#endif


#define Z_IS_ENABLED1(config_macro) Z_IS_ENABLED2(_XXXX##config_macro)
#define _XXXX1 _YYYY,
#define Z_IS_ENABLED2(one_or_two_args) Z_IS_ENABLED3(one_or_two_args 1, 0)
#define Z_IS_ENABLED3(ignore_this, val, ...) val
#define IS_ENABLED(config_macro) Z_IS_ENABLED1(config_macro)

#endif // CONF_H_
