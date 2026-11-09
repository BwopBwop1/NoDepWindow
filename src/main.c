

typedef unsigned char u8;
typedef unsigned short int u16;
typedef unsigned long int u32;
typedef unsigned long long int u64;

typedef signed char s8;
typedef signed short int s16;
typedef signed long int s32;
typedef signed long long int s64;

typedef float f32;
typedef double f64;

extern s32 syscall(s32 number, ...);

struct sockaddr {
  u16 sun_family;
  u8 sun_path[104];
};

struct pollfd {
  s32 fd;
  u16 events;
  u16 revents;
};

struct __attribute__((__packed__)) x11_connect_call {
  u8 order;
  u8 pad1;
  u16 major;
  u16 minor;
  u16 auth_proto_len;
  u16 auth_data_len;
  u16 pad2;
  // Optional Auth information if auth length are greater than 0.
};

struct timespec {
  u64 tv_sec;
  u64 tv_nsec;
};

// Syscalls
s64 write(u32 fd, const void *buf, u64 nbyte);
s64 read(u32 fd, const void *buf, u64 nbyte);
s32 socket(u32 domain, u32 type, u32 protocol);
s32 connect(s32 sockfd, struct sockaddr *addr, u64 addrlen);
s32 send(s32 sockfd, const void *buf, u64 size, u32 flags);
s32 shutdown(s32 sockfd, s32 how);
s32 close(s32 fd);
s32 poll(struct pollfd *fds, u64 nfds, s32 timeout);
s32 nanosleep(struct timespec *spec, struct timespec *rem);

u32 str_len(const u8 *msg);
void memzero(void *buf, u64 size);
void hsv_to_rgb(u16 h, u8 s, u8 v, u8 *r, u8 *g, u8 *b);


u32 globalID = 1;
u32 globalIDBase = 0;
u32 globalIDMask = 0;
u32 globalRootWin = 0;
u32 globalRootVisualID = 0;

// If I had access to malloc I would not do this.
// Instead I would use memory arena's
#define READBUFFER_SIZE 16*1024
#define SENDBUFFER_SIZE 16*1024
char ReadBuffer[READBUFFER_SIZE];
char SendBuffer[SENDBUFFER_SIZE];

u32 get_next_id();

#define AF_UNIX 1
#define SOCK_STREAM 1

#define SIZEOF_SOCKADDR_UN 110

#define PAD(N) ((4 - (N % 4)) % 4)

#define X11_GC_FLAG_BG (1 << 3)

#define X11_OP_CREATE_WINDOW 1
#define X11_OP_CHANGE_WIN_ATTR 2
#define X11_OP_MAP_WINDOW  8
#define X11_OP_CREATE_GC 55
#define X11_OP_CLEAR_AREA 61

#define X11_EVENT_MASK (1 << 11)
#define X11_BACKGROUND_PIXEL (1 << 1)

#define X11_EVENT_KEYPRESS (1 << 0)
#define X11_EVENT_KEYRELEASE (1 << 1)
#define X11_EVENT_EXPOSURE (1 << 15)
#define X11_EVENT_NOTIFY (1 << 17)

#define WINDOWCLASS_COPYFROMPARENT 0
#define WINDOWCLASS_INPUTOUTPUT 1
#define WINDOWCLASS_INPUTONLY 2

#define POLLIN (1 << 0)
#define POLLER (1 << 3)
#define POLLUP (1 << 4)

int main() {

  memzero(ReadBuffer, READBUFFER_SIZE);
  memzero(SendBuffer, SENDBUFFER_SIZE);
  
  s32 sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr x11_addr = {.sun_family = AF_UNIX, .sun_path = {"/tmp/.X11-unix/X0"}};
  s32 res = connect(sockfd, (struct sockaddr *)&x11_addr, SIZEOF_SOCKADDR_UN);
  if (res < 0) {
    const u8 *err = (const u8 *)("You got an error connecting!\n");
    write(1, err, str_len(err));
    close(sockfd);
    return 0;
  }

  struct x11_connect_call handshake = {0};
  handshake.order = 'l';
  handshake.major = 11;
  s32 bytes_write = write(sockfd, &handshake, sizeof(handshake));
  if (bytes_write != sizeof(handshake)) {
    const u8 *err = (const u8 *)("You got an error writing!\n");
    write(1, err, str_len(err));
    close(sockfd);
    return 0;
  }

  s32 bytes_read = read(sockfd, &ReadBuffer, 8);
  if (bytes_read != 8) {
    const u8 *err = (const u8 *)("You got an error reading!\n");
    close(sockfd);
    return 0;
  }

  if (ReadBuffer[0] == 0 || ReadBuffer[0] == 2) {
    const u8 *err = (const u8 *)("Either X11 expect auth or you failed the handshake!\n");
    write(1, err, str_len(err));
    close(sockfd);
    return 0;
  }
  
  bytes_read = read(sockfd, ReadBuffer + 8, READBUFFER_SIZE - 8);

  // NOTE: In the inital X11 connection response assuming you didn't get an error or 
  // an request to auth. You will end up getting a lot of information. However that
  // information is not really required to set up a basic window.
  // 
  // NOTE: In an actual application that probably links against libc and using xlib
  // It might be useful to have that information.
  //
  // NOTE: For this application the only neseccary information needed are the RootWindow,
  // RootVisualId, IDBase, and IDMask the reason for that is the Root Window and Visual ID
  // are used for window creation and the IDBase and IDMask give us a sort of name space for
  // the resources we register with the x11 server.
  
  globalIDBase = *((u32 *)&ReadBuffer[12]);
  globalIDMask = *((u32 *)&ReadBuffer[16]);
  u16 LengthOfVendor = *((u16 *)&ReadBuffer[24]);
  u8 NumberOfFormats = *((u8 *)&ReadBuffer[29]);
  u8 *Vendor = (u8 *)&ReadBuffer[40];

  s32 vendorPad = PAD(LengthOfVendor);
  s32 FormatByteLength = 8 * NumberOfFormats;
  s32 ScreensStartOffset = 40 + LengthOfVendor + vendorPad + FormatByteLength;

  u32 RootWindow = *((u32 *)&ReadBuffer[ScreensStartOffset]);
  u32 RootVisualId = *((u32 *)&ReadBuffer[ScreensStartOffset + 32]);

  u16 hue = 0;
  u8 r = 0;
  u8 g = 0;
  u8 b = 0;

  // NOTE: Something interesting to note above here is that we send a u16 with a size of 8 + 2
  // this is a length however it's not a length in just bytes it is a length in (bytes / 4) so 8 is the 
  // static number of (bytes / 4) in the call and 2 is the number of (bytes / 4) for the variable length
  // information. 
  
  u32 windowID = get_next_id();
  SendBuffer[0] = X11_OP_CREATE_WINDOW;
  SendBuffer[1] = 0;
  *((u16 *)&SendBuffer[2]) = (8 + 2);
  *((u32 *)&SendBuffer[4]) = windowID;
  *((u32 *)&SendBuffer[8]) = RootWindow;
  *((s16 *)&SendBuffer[12]) = 0;
  *((s16 *)&SendBuffer[14]) = 0;
  *((u16 *)&SendBuffer[16]) = 960;
  *((u16 *)&SendBuffer[18]) = 540;
  *((u16 *)&SendBuffer[20]) = 1;
  *((u16 *)&SendBuffer[22]) = WINDOWCLASS_INPUTOUTPUT;
  *((u32 *)&SendBuffer[24]) = RootVisualId;
  *((u32 *)&SendBuffer[28]) = X11_EVENT_MASK | X11_BACKGROUND_PIXEL;
  *((u32 *)&SendBuffer[32]) = 255 << 24 | r << 16 | g << 8 | b;
  *((u32 *)&SendBuffer[36]) = X11_EVENT_EXPOSURE | X11_EVENT_KEYPRESS | X11_EVENT_NOTIFY;

  bytes_write = write(sockfd, (char *)&SendBuffer, (8 * 2)*4);

  // NOTE: The u16 length is a static size of 2 (bytes / 4)
  SendBuffer[0] = X11_OP_MAP_WINDOW;
  SendBuffer[1] = 0;
  *((u16 *)&SendBuffer[2]) = 2;
  *((u32 *)&SendBuffer[4]) = windowID;

  bytes_write = write(sockfd, (char *)&SendBuffer, 2 * 4);
  
  struct pollfd PollDescriptors[1] = {};
  PollDescriptors[0].fd = sockfd;
  PollDescriptors[0].events = POLLIN;
  s32 DescriptorCount = 1;
  u32 IsProgramRunning = 1;

  while (IsProgramRunning) {
    s32 EventCount = poll(PollDescriptors, DescriptorCount, 0);
    u32 read_bytes = 0;
    if (EventCount > 0) {
      read_bytes = read(sockfd, &ReadBuffer, READBUFFER_SIZE);
    }


    if (PollDescriptors[0].revents & POLLER) {
      const u8 *err = (const u8 *)("There was an error!\n");
      write(1, err, str_len(err));
      close(sockfd);
      IsProgramRunning = 0;
    }

    if (PollDescriptors[0].revents & POLLUP) {
      const u8 *err = (const u8 *)("Socket Connection closed!\n");
      write(1, err, str_len(err));
      close(sockfd);
      IsProgramRunning = 0;
    }
    
    hsv_to_rgb(hue, 255, 255, &r, &g, &b);

    // NOTE: The u16 length is a static size of 3 (bytes / 4 ) 
    // + the variable length of 2 (bytes / 4) .
    // Which should resemble the create window call.
    SendBuffer[0] = X11_OP_CHANGE_WIN_ATTR;
    SendBuffer[1] = 0;
    *((u16 *)&SendBuffer[2]) = (3 + 2);
    *((u32 *)&SendBuffer[4]) = windowID;
    *((u32 *)&SendBuffer[8]) = X11_EVENT_MASK | X11_BACKGROUND_PIXEL;
    *((u32 *)&SendBuffer[12]) = 255 << 24 | r << 16 | g << 8 | b;
    *((u32 *)&SendBuffer[16]) = X11_EVENT_EXPOSURE | X11_EVENT_KEYPRESS | X11_EVENT_NOTIFY;

    write(sockfd, (char *)&SendBuffer, (3+2) * 4);

    hue++;
    if (hue >= 360) {
      hue = 0;
    }

    struct timespec time = {0};
    time.tv_sec = 0;
    time.tv_nsec = 20000000;
    struct timespec rem = {0};
    nanosleep(&time, &rem);

    // NOTE: The u16 length is a static size of 4 (bytes / 4).
    SendBuffer[0] = X11_OP_CLEAR_AREA;
    SendBuffer[1] = 0;
    *((u16 *)&SendBuffer[2]) = 4;
    *((u32 *)&SendBuffer[4]) = windowID;
    *((s16 *)&SendBuffer[8]) = 0;
    *((s16 *)&SendBuffer[10]) = 0;
    *((u16 *)&SendBuffer[12]) = 0;
    *((u16 *)&SendBuffer[14]) = 0;

    write(sockfd, (char *)&SendBuffer, (4) * 4);
    
  }

  return 0;
}

// Based off this.
// https://www.programmingalgorithms.com/algorithm/hsv-to-rgb/c/
void hsv_to_rgb(u16 h, u8 s, u8 v, u8 *r, u8 *g, u8 *b) {
  u16 region = h / 60;
  u16 remainder = (h - (region * 60)) * 255 / 60;

  u8 p = (v * (255 - s)) / 255;
  u8 q = (v * (255 - ((s * remainder) / 255))) / 255;
  u8 t = (v * (255 - ((s * (255 - remainder)) / 255))) / 255;

  switch (region) {
    case 0:
      *r = v; *g = t; *b = p;
      break;
    case 1:
      *r = q; *g = v; *b = p;
      break;
    case 2:
      *r = p; *g = v; *b = t;
      break;
    case 3:
      *r = p; *g = q; *b = v;
      break;
    case 4:
      *r = t; *g = p; *b = v;
      break;
    default:
      *r = v; *g = p; *b = q;
      break;
  }
}

s32 nanosleep(struct timespec *spec, struct timespec *rem) {
  return syscall(35, spec, rem);
}

s64 read(u32 fd, const void *buf, u64 nbyte) {
  return syscall(0, fd, buf, nbyte);
}

s64 write(u32 fd, const void *buf, u64 nbyte) {
  return syscall(1, fd, buf, nbyte);
}

s32 socket(u32 domain, u32 type, u32 protocol) {
  return syscall(41, domain, type, protocol);
}

s32 connect(s32 sockfd, struct sockaddr *addr, u64 addrlen) {
  return syscall(42, sockfd, addr, addrlen);
}

s32 send(s32 sockfd, const void *buf, u64 size, u32 flags) {
  return syscall(44, sockfd, buf, size, flags, 0, 0);
}

s32 shutdown(s32 sockfd, s32 how) {
  return syscall(48, sockfd, how);
}

s32 close(s32 fd) {
  return syscall(3, fd);
}

s32 poll(struct pollfd *fds, u64 nfds, s32 timeout) {
  return syscall(7, fds, nfds, timeout);
}

void memzero(void *buf, u64 size) {
  u8* single_byte = buf;
  for (u64 i = 0; i < size; i++) {
    single_byte[i] = 0;
  }
}

u32 str_len(const u8 *msg) {
  u32 result = 0;
  while (*msg) {
    msg++;
    result++;
  }
  return result;
}

u32 get_next_id() {
  return globalIDMask & (globalID++) | globalIDBase;
}
