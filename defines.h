#define MEM_SIZE 0x10000

#define ROM_SIZE 0x100

#define ADDR_IE 0xFFFF // Interrupt enable.
#define ADDR_IF 0xFF0F // Interrupt flag.
#define ADDR_VIDEO_START 0x8000
#define ADDR_VIDEO_END   0x9FFF

#define SHELL_TXT_ERROR "\033[1m\033[101m "
#define SHELL_TXT_RESET_NL " \033[0m\n"
#define ERR(x) printf(SHELL_TXT_ERROR); x; printf(SHELL_TXT_RESET_NL)
