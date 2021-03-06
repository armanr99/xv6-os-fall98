// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void consputc(int, int);

static int panicked = 0;

static struct {
  struct spinlock lock;
  int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i], 1);
}
//PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint*)(void*)(&fmt + 1);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c, 1);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s, 1);
      break;
    case '%':
      consputc('%', 1);
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%', 1);
      consputc(c, 1);
      break;
    }
  }

  if(locking)
    release(&cons.lock);
}

void
panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for(;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c, int cp)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  if(c == '\n')
    pos += 80 - pos%80;
  else if(c == BACKSPACE){
    if(pos > 0 && cp == 1) --pos;
  } else if (c == '{'){
    pos -= pos % 80;
    pos += 5;
    crt[pos] = (c&0xff) | 0x0700;  // black on white !!
    pos += cp;
  } else if (c == '}'){
  } else {
    crt[pos] = (c&0xff) | 0x0700;  // black on white
    pos += cp;
  }

  if(pos < 0 || pos > 25*80)
    panic("pos under/overflow");

  if((pos/80) >= 24){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*23*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos] = ' ' | 0x0700;
}

void
consputc(int c, int cp)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }

  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else
    uartputc(c);
  cgaputc(c, cp);
}

#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
  uint ei; // Last Char in buf index
} input;

#define C(x)  ((x)-'@')  // Control-x

void
myprintint(int xx)
{
  printint(xx, 10, 0);
}

void crt_erase_char(int back_counter){
  int pos;

  // get cursor position
  outb(CRTPORT, 14);                  
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  for(int i = pos-1; i <= pos + back_counter; i++)
    crt[i] = crt[i+1];

  pos--;

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos+back_counter] = ' ' | 0x0700;
}


void crt_insert_char(int c, int back_counter){
  int pos;

  // get cursor position
  outb(CRTPORT, 14);                  
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  for(int i = pos + back_counter; i >= pos; i--){
    crt[i+1] = crt[i];
  }
  crt[pos] = (c&0xff) | 0x0700;  

  pos++;

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos+back_counter] = ' ' | 0x0700;
}

void move_back_cursor(int cnt){
  int pos;
  
  // get cursor position
  outb(CRTPORT, 14);                  
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);    

  pos -= cnt;

  outb(CRTPORT, 15);
  outb(CRTPORT+1, (unsigned char)(pos&0xFF));
  outb(CRTPORT, 14);
  outb(CRTPORT+1, (unsigned char )((pos>>8)&0xFF));
}

void move_forward_cursor(int cnt){
  int pos;
  
  // get cursor position
  outb(CRTPORT, 14);                  
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);    

  pos += cnt;

  outb(CRTPORT, 15);
  outb(CRTPORT+1, (unsigned char)(pos&0xFF));
  outb(CRTPORT, 14);
  outb(CRTPORT+1, (unsigned char )((pos>>8)&0xFF));
}

int back_counter = 0;

void
consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while((c = getc()) >= 0){
    switch(c){
    case '{':
      input.e = input.w;
      consputc('{', 1); consputc(BACKSPACE, 1);
      back_counter += (input.ei - input.e) + 1;
      move_back_cursor(input.ei - input.e);
      break;
    case '}':
      move_forward_cursor(input.ei - input.e);
      input.e = input.ei;
      back_counter = 0;
      break;
    case C('P'):  // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('C'): case C('U'):  // Kill line.
      move_forward_cursor(input.ei - input.e);
      while(input.ei != input.w &&
            input.buf[(input.ei-1) % INPUT_BUF] != '\n'){
        if(input.e > input.w)
          input.e--;
        input.ei--;
        consputc(BACKSPACE, 1);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(input.e != input.w){
        int i;
        if(input.e != input.ei){
          for(i = input.e-1; i < input.ei; i++)
            input.buf[i % INPUT_BUF] = input.buf[(i+1) % INPUT_BUF];
        }
        input.e--;
        input.ei--;
        crt_erase_char(back_counter);
      }
      break;
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        if(c == '\n' || c == C('D') || input.ei == input.r+INPUT_BUF){
          input.buf[input.ei++ % INPUT_BUF] = c;
          consputc(c, 1);
          input.e = input.ei;
          input.w = input.e;
          wakeup(&input.r);
          break;
        }
        if(input.e != input.ei){
          uint i;
          for(i = input.ei; i >= input.e; i--)
            input.buf[i % INPUT_BUF] = input.buf[(i-1) % INPUT_BUF];
          input.buf[input.e++ % INPUT_BUF] = c;
          input.ei++;
          crt_insert_char(c, back_counter);
        }
        else{
          input.buf[input.e++ % INPUT_BUF] = c;
          input.ei++;
          crt_insert_char(c, back_counter);
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
}

int
consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while(n > 0){
    while(input.r == input.w){
      if(myproc()->killed){
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff, 1);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void
consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
}

