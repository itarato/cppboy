#define MEM_SIZE 0x10000

#define ROM_SIZE 0x100

#define ADDR_IE 0xFFFF // Interrupt enable.
#define ADDR_IF 0xFF0F // Interrupt flag.
#define ADDR_VIDEO_START 0x8000
#define ADDR_VIDEO_END   0x9FFF
#define ADDR_LCDC 0xFF40 // RW
#define ADDR_STAT 0xFF41 // RW
#define ADDR_DIV  0xFF04 // RW
#define ADDR_TAC  0xFF07 // RW
#define ADDR_TMA  0xFF06 // RW
#define ADDR_TIMA 0xFF05 // RW
#define ADDR_IF   0xFF0F

#define SHELL_TXT_ERROR "\033[1m\033[101m "
#define SHELL_TXT_RESET_NL " \033[0m\n"
#define ERR(x) printf(SHELL_TXT_ERROR); x; printf(SHELL_TXT_RESET_NL)

#define BITN(v, n) (((v) >> (n)) & 0b1)
#define ISBITN(v, n) (BITN(v, n) == 1)

#define BITFZ 7
#define BITFN 6
#define BITFH 5
#define BITFC 4

// #define LOG_LEVEL_DEBUG
// #define LOG_LEVEL_INFO
// #define LOG_LEVEL_NOTICE

#define DEBUG

#ifdef LOG_LEVEL_DEBUG
  #define LOG_DEBUG(f) f
#else
  #define LOG_DEBUG(f) void()
#endif

#ifdef LOG_LEVEL_INFO
  #define LOG_INFO(f) f
#else
  #define LOG_INFO(f) void()
#endif

#ifdef LOG_LEVEL_NOTICE
  #define LOG_NOTICE(f) f
#else
  #define LOG_NOTICE(f) void()
#endif
