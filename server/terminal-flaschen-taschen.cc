// -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>

#include "led-flaschen-taschen.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define SCREEN_CLEAR    "\033c"
#define SCREEN_PREFIX   "\033[48;2;0;0;0m"  // set black background
#define SCREEN_POSTFIX  "\033[0m"           // reset terminal settings
#define SCREEN_CURSOR_UP_FORMAT "\033[%dA"  // Move cursor up given lines.
#define CURSOR_OFF      "\033[?25l"
#define CURSOR_ON       "\033[?25h"

// Each character on the screen is divided in a top pixel and bottom pixel.
// We use the following character which fills the top block:
#define PIXEL_CHARACTER  "▀"  // Top foreground color, bottom background color

// Now, pixels on the even row will get the foreground color changed, pixels
// on odd rows the background color. Two pixels one stone. Or something.
#define ESCAPE_COLOR_FORMAT   "%03d;%03d;%03d"
#define TOP_PIXEL_COLOR       "\033[38;2;"
#define BOTTOM_PIXEL_COLOR    "\033[48;2;"

// Displaying the framerate at the bottom of the screen. Keep spaceholder.
#define FPS_PLACEHOLDER "___________"

TerminalFlaschenTaschen::TerminalFlaschenTaschen(int fd, int w, int h)
    // Height is rounded up to the next even number.
    : terminal_fd_(fd), width_(w), height_((h + 1) & ~0x1), is_first_(true),
      last_time_(-1) {
    buffer_.append(SCREEN_PREFIX);
    initial_offset_ = buffer_.size();
    char scratch[64];
    snprintf(scratch, sizeof(scratch),
             TOP_PIXEL_COLOR    ESCAPE_COLOR_FORMAT "m"
             BOTTOM_PIXEL_COLOR ESCAPE_COLOR_FORMAT "m"
             PIXEL_CHARACTER,
             0, 0, 0, 0, 0, 0); // black.
    pixel_offset_ = strlen(scratch);
    for (int y = 0; y < height_ / 2; ++y) {
        for (int x = 0; x < width_; ++x) {
            buffer_.append(scratch);
        }
        buffer_.append("\n");
    }

    buffer_.append(SCREEN_POSTFIX);

    fps_offset_ = buffer_.size();
    buffer_.append(FPS_PLACEHOLDER "\n\n");

    snprintf(scratch, sizeof(scratch), SCREEN_CURSOR_UP_FORMAT, height_/2 + 2);
    buffer_.append(scratch);

    // Some useful precalculated lengths.
    snprintf(scratch, sizeof(scratch), ESCAPE_COLOR_FORMAT, 0, 0, 0);
    color_fmt_length_ = strlen(scratch);

    snprintf(scratch, sizeof(scratch),
             ESCAPE_COLOR_FORMAT "m" BOTTOM_PIXEL_COLOR, 0, 0, 0);
    lower_row_pixel_offset_ = strlen(scratch);
}
TerminalFlaschenTaschen::~TerminalFlaschenTaschen() {
    if (!is_first_) {
        write(terminal_fd_, SCREEN_CLEAR, strlen(SCREEN_CLEAR));
        write(terminal_fd_, CURSOR_ON, strlen(CURSOR_ON));
    }
}

void TerminalFlaschenTaschen::SetPixel(int x, int y, const Color &col) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    const int double_row = y/2;
    const int pos = initial_offset_
        + (width_ * double_row + x) * pixel_offset_
        + strlen(TOP_PIXEL_COLOR)            // go where the color fmt starts
        + (y % 2) * lower_row_pixel_offset_  // offset for odd-row y-pixels
        + double_row;                        // 1 newline per double row
    char *buf = const_cast<char*>(buffer_.data()) + pos;  // Living on the edge
    snprintf(buf, color_fmt_length_+1, ESCAPE_COLOR_FORMAT, col.r, col.g, col.b);
    buf[color_fmt_length_] = 'm';   // overwrite \0-byte with closing 'm' again.
}

void TerminalFlaschenTaschen::Send() {
    if (is_first_) {
        write(terminal_fd_, SCREEN_CLEAR, strlen(SCREEN_CLEAR));
        write(terminal_fd_, CURSOR_OFF, strlen(CURSOR_OFF));
        is_first_ = false;
    }

    char *fps_place = const_cast<char*>(buffer_.data()) + fps_offset_;
    struct timeval tp;
    gettimeofday(&tp, NULL);
    const int64_t time_now_us = tp.tv_sec * 1000000 + tp.tv_usec;
    const int64_t duration = time_now_us - last_time_;
    if (last_time_ > 0 && duration > 500 && duration < 10000000) {
        const float fps = 1e6 / duration;
        snprintf(fps_place, strlen(FPS_PLACEHOLDER)+1, "%7.1f fps", fps);
    } else {
        memcpy(fps_place, FPS_PLACEHOLDER, strlen(FPS_PLACEHOLDER));
    }
    last_time_ = time_now_us;

    write(terminal_fd_, buffer_.data(), buffer_.size());
}
