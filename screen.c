#include "screen.h"

#define COLOR_BLACK           0
#define COLOR_BLUE            1
#define COLOR_GREEN           2
#define COLOR_CYAN            3
#define COLOR_RED             4
#define COLOR_MAGENTA         5
#define COLOR_BROWN           6
#define COLOR_LIGHT_GREY      7
#define COLOR_DARK_GREY       8
#define COLOR_LIGHT_BLUE      9
#define COLOR_LIGHT_GREEN    10
#define COLOR_LIGHT_CYAN     11
#define COLOR_LIGHT_RED      12
#define COLOR_LIGHT_MAGENTA  13
#define COLOR_LIGHT_BROWN    14
#define COLOR_WHITE          15

#define COLOR(foreground,background)  ((background) << 4 | (foreground))

#define NUM_LINES 24
#define NUM_COLS 80

static int curr_line;
static int curr_col;


static void
setchar(int line, int col, int c)
{
  unsigned char *videoram = (unsigned char *)0xB8000;
  unsigned int idx = 2 * (line * NUM_COLS + col);
  videoram[idx] = c;
  videoram[idx + 1] = COLOR(COLOR_GREEN, COLOR_BLACK);
}


static void
clear_line(int line)
{
  for (int col = 0; col < NUM_COLS; col++)
    setchar(line, col, ' ');
}


void
clear_screen(void)
{
  for (int line = 0; line < NUM_LINES; line++)
    clear_line(line);
  curr_line = 0;
  curr_col = 0;
}


void *
memset(void *s, int c, unsigned int n)
{
  char *p = s;
  for (unsigned int i = 0; i < n; i++)
    *p++ = c;
  return s;
}


static void
putchar(int c)
{
  if (c == '\n')
    {
      curr_col = 0;
      if (++curr_line == NUM_LINES)
	curr_line = 0; 
    }
  else
    if (curr_col < NUM_COLS)
      setchar(curr_line, curr_col++, c);
}


static void
print_string(const char *s)
{
  while (*s)
    putchar(*s++);
}


static void
print_uint_helper(unsigned int u)
{
  if (u != 0)
    {
      print_uint_helper(u / 10);
      putchar('0' + u % 10);
    }
}


static void
print_uint(unsigned int u)
{
  if (u == 0)
    putchar('0');
  else
    print_uint_helper(u);
}


static void
print_hex(unsigned int u)
{
  for (int i = 7; i >= 0; i--)
    {
      const char *c = "0123456789abcdef";
      putchar(c[(u >> (i * 4)) & 0xf]);
    }
}


static void
print_HEX(unsigned int u)
{
  for (int i = 7; i >= 0; i--)
    {
      const char *c = "0123456789ABCDEF";
      putchar(c[(u >> (i * 4)) & 0xf]);
    }
}


static void
print_int(int i)
{
  if (i < 0)
    {
      putchar('-');
      print_uint(-i);
    }
  else
    print_uint(i);
}


void
printf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  while (*fmt)
    {
      if (*fmt == '%')
	{
	  switch (*(++fmt))
	    {
	    case 'd':
	    case 'i':
	      fmt++;
	      print_int(va_arg(ap, int));
	      break;
	    case 'p':
	      fmt++;
	      putchar('0');
	      putchar('x');
	      print_hex((unsigned int)va_arg(ap, void *));
	      break;
	    case 's':
	      fmt++;
	      print_string(va_arg(ap, char *));
	      break;
	    case 'u':
	      fmt++;
	      print_uint(va_arg(ap, unsigned int));
	      break;
	    case 'x':
	      fmt++;
	      print_hex(va_arg(ap, unsigned int));
	      break;
	    case 'X':
	      fmt++;
	      print_HEX(va_arg(ap, unsigned int));
	      break;
	    default:
	      putchar('%');
	      if (*fmt)
		putchar(*fmt++);
	    }
	}
      else
	putchar(*fmt++);
    }
  va_end(ap);
}
