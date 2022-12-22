#include <string.h>

#define mp_hal_stdout_tx_str my_stdout_str
#define mp_hal_stdout_tx_strn my_stdout_strn
#define mp_hal_stdout_tx_strn_cooked my_stdout_strn

static inline void mp_hal_set_interrupt_char(char c)
{
    
}

void my_stdout_strn(const char *, size_t);

// Send zero-terminated string
static inline void my_stdout_str(const char *str) {
    my_stdout_strn(str, strlen(str));
}
