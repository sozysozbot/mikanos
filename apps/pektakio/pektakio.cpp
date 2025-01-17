#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <tuple>
#include <unistd.h>
#include <vector>
#include "../syscall.h"

std::tuple<int, char*, size_t> MapFile(const char* filepath) {
  SyscallResult res = SyscallOpenFile(filepath, O_RDONLY);
  if (res.error) {
    fprintf(stderr, "%s: %s\n", strerror(res.error), filepath);
    exit(1);
  }

  const int fd = res.value;
  size_t filesize;
  res = SyscallMapFile(fd, &filesize, 0);
  if (res.error) {
    fprintf(stderr, "%s\n", strerror(res.error));
    exit(1);
  }

  return {fd, reinterpret_cast<char*>(res.value), filesize};
}

int margin_left = 4;
int margin_top = 24;

uint64_t OpenTextWindow(int width_in_pixel, int height_in_pixel, const char* title) {
  SyscallResult res = SyscallOpenWindow(4 + margin_left + width_in_pixel, 4 + margin_top + height_in_pixel, 10, 10, title);
  if (res.error) {
    fprintf(stderr, "%s\n", strerror(res.error));
    exit(1);
  }
  const uint64_t layer_id = res.value;

  auto fill_rect = [layer_id](int x, int y, int w, int h, uint32_t c) {
    SyscallWinFillRectangle(layer_id, x, y, w, h, c);
  };
  fill_rect(3,                  23,                   1 + width_in_pixel, 1,                   0x666666);
  fill_rect(3,                  24,                   1,                  1 + height_in_pixel, 0x666666);
  fill_rect(4,                  25 + height_in_pixel, 1 + width_in_pixel, 1,                   0xcccccc);
  fill_rect(5 + width_in_pixel, 24,                   1,                  1 + height_in_pixel, 0xcccccc);

  return layer_id;
}

struct Line {
  const char* line;
  size_t line_len;
  bool is_underlined;
};

using LinesType = std::vector<Line>;

LinesType FindLines(const char* p, size_t len) {
  LinesType lines;
  const char* end = p + len;
  bool is_underlined = false;
  bool continue_underlined = false;

  auto next_lf = [end, &is_underlined, &continue_underlined](const char* s) {
    // '#' at the beginning of a line should make the line into a header.
    is_underlined = continue_underlined || *s == '#';
    continue_underlined = false;

    if (*s == '#') {
      ++s;
    }

    while (s < end && *s != '\n' && *s != '%') { 
      // In this document format, the percent sign is used to the "soft-return",
      // that is, "the automatic change of line due to reaching the end of the line"
      // この文書フォーマットでは、パーセント記号はソフトリターン、つまり
      // 「行末に達したときに自動で行われるような改行」を表す。
      ++s;
    }
    if (*s == '%' && is_underlined) {
      // soft return should not cancel the effect of #
      continue_underlined = true;
    }
    return std::pair<const char*, bool>(s, is_underlined);
  };

  auto [lf, underline] = next_lf(p);
  while (lf < end) {
    const char *q = *p == '#' ? p + 1 : p;
    Line l = {q, static_cast<size_t>(lf - q), underline};
    lines.push_back(l);
    p = lf + 1;
    auto [lf_, underline_] = next_lf(p);
    lf = lf_; 
    underline = underline_;
  }
  if (p < end) {
    const char *q = *p == '#' ? p + 1 : p;
    Line l = {q, static_cast<size_t>(end - q), underline};
    lines.push_back(l);
  }

  return lines;
}

int CountUTF8Size(uint8_t c) {
  if (c < 0x80) {
    return 1;
  } else if (0xc0 <= c && c < 0xe0) {
    return 2;
  } else if (0xe0 <= c && c < 0xf0) {
    return 3;
  } else if (0xf0 <= c && c < 0xf8) {
    return 4;
  }
  return 0;
}

std::pair<char32_t, int> ConvertUTF8To32(const char* u8) {
  switch (CountUTF8Size(u8[0])) {
  case 1:
    return {
      static_cast<char32_t>(u8[0]),
      1
    };
  case 2:
    return {
      (static_cast<char32_t>(u8[0]) & 0b0001'1111) << 6 |
      (static_cast<char32_t>(u8[1]) & 0b0011'1111) << 0,
      2
    };
  case 3:
    return {
      (static_cast<char32_t>(u8[0]) & 0b0000'1111) << 12 |
      (static_cast<char32_t>(u8[1]) & 0b0011'1111) << 6 |
      (static_cast<char32_t>(u8[2]) & 0b0011'1111) << 0,
      3
    };
  case 4:
    return {
      (static_cast<char32_t>(u8[0]) & 0b0000'0111) << 18 |
      (static_cast<char32_t>(u8[1]) & 0b0011'1111) << 12 |
      (static_cast<char32_t>(u8[2]) & 0b0011'1111) << 6 |
      (static_cast<char32_t>(u8[3]) & 0b0011'1111) << 0,
      4
    };
  default:
    return { 0, 0 };
  }
}

void CopyUTF8String(char* dst, size_t dst_size,
                    const char* src, size_t src_size,
                    /*int w,*/ int tab) {
  int x = 0;

  const auto src_end = src + src_size;
  const auto dst_end = dst + dst_size;
  while (*src) {
    if (*src == '\t') {
      int spaces = tab - (x % tab);
      if (dst + spaces >= dst_end) {
        break;
      }
      memset(dst, ' ', spaces);
      ++src;
      dst += spaces;
      x += spaces;
      continue;
    }

    const auto [ u32, bytes ] = ConvertUTF8To32(src);
    SyscallResult is_halfwidth = SyscallIsHalfwidth(u32);
    x += is_halfwidth.value ? 1 : 2;

    /*if (x > w) {
      break;
    }*/

    if (src + bytes > src_end || dst + bytes >= dst_end) {
      break;
    }
    memcpy(dst, src, bytes);
    src += bytes;
    dst += bytes;
  }

  *dst = '\0';
}

int font_height = 24;
int width_in_pixel = 592;

void DrawLines(const LinesType& lines, int start_line,
               uint64_t layer_id, int width_in_pixel, int height_in_pixel, int tab) {
  char buf[1024];
  SyscallWinFillRectangle(layer_id, margin_left, margin_top, width_in_pixel, height_in_pixel, 0xffffff);
  int padding_left = width_in_pixel / 8;

  for (int i = 0; i * font_height < height_in_pixel; ++i) {
    int line_index = start_line + i;
    if (line_index < 0 || lines.size() <= line_index) {
      continue;
    }
    Line l = lines[line_index];
    auto line = l.line;
    auto line_len = l.line_len;
    bool underlined = l.is_underlined;
    CopyUTF8String(buf, sizeof(buf), line, line_len, /*w,*/ tab);
    SyscallResult res = SyscallWinWriteStringInPektak(layer_id, margin_left + padding_left, margin_top + font_height * i, 0x000000, buf, font_height);
    auto resulting_width = res.value;
    if (underlined) {
      SyscallWinFillRectangle(layer_id, margin_left + padding_left, margin_top + font_height * (i + 0.92), resulting_width, 2, 0x000000);
    }
  }
}

std::tuple<bool, int> WaitEvent(int h) {
  AppEvent events[1];
  while (true) {
    auto [ n, err ] = SyscallReadEvent(events, 1);
    if (err) {
      fprintf(stderr, "mi nix mels xelo voleso. %s\n", strerror(err));
      return {false, 0};
    }
    if (events[0].type == AppEvent::kQuit) {
      return {true, 0};
    } else if (events[0].type == AppEvent::kKeyPush &&
               events[0].arg.keypush.press) {
      return {false, events[0].arg.keypush.keycode};
    }
  }
}

bool UpdateStartLine(int* start_line, int height, size_t num_lines) {
  while (true) {
    const auto [ quit, keycode ] = WaitEvent(height);
    if (quit) {
      return quit;
    }
    if (num_lines < height) {
      continue;
    }

    int diff;
    switch (keycode) {
    case 75: diff = -height/2; break; // PageUp
    case 78: diff =  height/2; break; // PageDown
    case 81: diff =  1;        break; // DownArrow
    case 82: diff = -1;        break; // UpArrow
    default:
      continue;
    }

    if ((diff < 0 && *start_line == 0) ||
        (diff > 0 && *start_line == num_lines - height)) {
      continue;
    }

    *start_line += diff;
    if (*start_line < 0) {
      *start_line = 0;
    } else if (*start_line > num_lines - height) {
      *start_line = num_lines - height;
    }
    return false;
  }
}

extern "C" void main(int argc, char** argv) {
  auto print_help = [argv](){
    fprintf(stderr,
            "liusel: %s [-w 横] [-h 縦] [-t konlaval] <chertif>\n",
            argv[0]);
  };

  int opt;
  int height_in_pixel = 416, tab = 8;
  while ((opt = getopt(argc, argv, "w:h:t:")) != -1) {
    switch (opt) {
    case 'w': width_in_pixel = atoi(optarg); break;
    case 'h': height_in_pixel = atoi(optarg); break;
    case 't': tab = atoi(optarg); break;
    default:
      print_help();
      exit(1);
    }
  }
  if (optind >= argc) {
    print_help();
    exit(1);
  }

  const char* filepath = argv[optind];
  const auto [ fd, content, filesize ] = MapFile(filepath);

  const char* last_slash = strrchr(filepath, '/');
  const char* filename = last_slash ? &last_slash[1] : filepath;
  const auto layer_id = OpenTextWindow(width_in_pixel, height_in_pixel, filename);

  const auto lines = FindLines(content, filesize);

  int start_line = 0;
  while (true) {
    DrawLines(lines, start_line, layer_id, width_in_pixel, height_in_pixel, tab);
    if (UpdateStartLine(&start_line, height_in_pixel / font_height, lines.size())) {
      break;
    }
  }

  SyscallCloseWindow(layer_id);
  exit(0);
}
