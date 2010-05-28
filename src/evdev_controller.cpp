/*
**  Xbox360 USB Gamepad Userspace Driver
**  Copyright (C) 2010 Ingo Ruhnke <grumbel@gmx.de>
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "evdev_controller.hpp"

#include <linux/input.h>
#include <stdexcept>
#include <string.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)

EvdevController::EvdevController(const std::string& filename) :
  fd(-1)
{
  memset(abs2idx, 0, sizeof(abs2idx));
  memset(rel2idx, 0, sizeof(rel2idx));
  memset(key2idx, 0, sizeof(key2idx));

  fd = open(filename.c_str(), O_RDONLY | O_NONBLOCK);

  if (fd == -1)
  {
    throw std::runtime_error(filename + ": " + std::string(strerror(errno)));
  }

  { // Get the human readable name
    char c_name[1024] = "unknown";
    ioctl(fd, EVIOCGNAME(sizeof(c_name)), c_name);
    name = c_name;
    std::cout << "Name: " << name << std::endl;
  }

  { // Read in how many btn/abs/rel the device has
    unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
    memset(bit, 0, sizeof(bit));
    ioctl(fd, EVIOCGBIT(0, EV_MAX), bit[0]);

    unsigned long abs_bit[NBITS(ABS_MAX)];
    unsigned long rel_bit[NBITS(REL_MAX)];
    unsigned long key_bit[NBITS(KEY_MAX)];

    memset(abs_bit, 0, sizeof(abs_bit));
    memset(rel_bit, 0, sizeof(rel_bit));
    memset(key_bit, 0, sizeof(key_bit));

    ioctl(fd, EVIOCGBIT(EV_ABS, ABS_MAX), abs_bit);
    ioctl(fd, EVIOCGBIT(EV_REL, REL_MAX), rel_bit);
    ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), key_bit);

    for(int i = 0; i < ABS_MAX; ++i)
      {
        if (test_bit(i, abs_bit))
          {
            struct input_absinfo absinfo;
            ioctl(fd, EVIOCGABS(i), &absinfo);
            std::cout << "Abs: " << i << " min: " << absinfo.minimum << " max: " << absinfo.maximum << std::endl;
            //abs2idx[i] = abs_port_out.size();
            //abs_port_out.push_back(new AbsPortOut("EvdevDriver:abs", absinfo.minimum, absinfo.maximum));
          }
      }

    for(int i = 0; i < REL_MAX; ++i)
      {
        if (test_bit(i, rel_bit))
          {
            std::cout << "Rel: " << i << std::endl;
            //rel2idx[i] = rel_port_out.size();
            //rel_port_out.push_back(new RelPortOut("EvdevDriver:rel"));
          }
      }

    for(int i = 0; i < KEY_MAX; ++i)
      {
        if (test_bit(i, key_bit))
          {
            //key2idx[i] = btn_port_out.size();
            //btn_port_out.push_back(new BtnPortOut("EvdevDriver:btn"));
          }
      }
  }
}

void
EvdevController::set_rumble(uint8_t left, uint8_t right)
{
  // not implemented
}

void
EvdevController::set_led(uint8_t status)
{
  // not implemented
}

bool
EvdevController::apply(XboxGenericMsg& msg, const struct input_event& ev)
{
  switch(ev.type)
  {
    case EV_KEY:
      {
        KeyMap::iterator it = key_map.find(ev.code);
        if (it != key_map.end())
        {
          set_button(msg, it->second, ev.value);
          return true;
        }
        else
        {
          return false;
        }
      }
      break;

    case EV_ABS:
      {
        AbsMap::iterator it = abs_map.find(ev.code);
        if (it != abs_map.end())
        {
          // FIXME: need to normalise the value to the proper range
          set_axis(msg, it->second, ev.value);
          return true;
        }
        else
        {
          return false;
        }
      }
      break;

    default:
      // not supported event
      return false;
      break;
  }
}

bool
EvdevController::read(XboxGenericMsg& msg, bool verbose, int timeout)
{
  bool successfull_read = false;

  struct input_event ev[128];

  // FIXME: We might need to temporary buffer events and not send them
  // instantly, as we might miss events otherwise, do joysticks send
  // out 'sync'?
  int rd = 0;
  while((rd = ::read(fd, ev, sizeof(struct input_event) * 128)) > 0)
  {
    for (int i = 0; i < rd / (int)sizeof(struct input_event); ++i)
    {
      successfull_read |= apply(msg, ev[i]);
    }
  }

  return successfull_read;
}

/* EOF */
