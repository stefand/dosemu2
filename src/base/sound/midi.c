/*
 *  Copyright (C) 2006 Stas Sergeev <stsp@users.sourceforge.net>
 *
 * The below copyright strings have to be distributed unchanged together
 * with this file. This prefix can not be modified or separated.
 */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "emu.h"
#include "ringbuf.h"
#include "timers.h"
#include "sound/sound.h"
#include "sound/midi.h"

#define MAX_OUT_PLUGINS 15
/* support only 1 input plugin for now to avoid the concurrent writes */
#define MAX_IN_PLUGINS 1
static struct pcm_holder out[MAX_OUT_PLUGINS];
static struct pcm_holder in[MAX_IN_PLUGINS];
#define OUT_PLUGIN(i) ((struct midi_out_plugin *)out[i].plugin)
#define IN_PLUGIN(i) ((struct midi_in_plugin *)in[i].plugin)
static int out_registered = 0, in_registered = 0;
static struct rng_s midi_in;

void midi_write(unsigned char val)
{
  int i;
  for (i = 0; i < out_registered; i++)
    if (out[i].initialized)
      OUT_PLUGIN(i)->write(val);
//  idle(0, 0, 0, "midi");
}

void midi_init(void)
{
  rng_init(&midi_in, 64, 1);
  pcm_init_plugins(out, out_registered);
  pcm_init_plugins(in, in_registered);
}

void midi_done(void)
{
  int i;
  for (i = 0; i < out_registered; i++) {
    if (out[i].initialized) {
      if (OUT_PLUGIN(i)->stop)
        OUT_PLUGIN(i)->stop();
      out[i].plugin->close(out[i].arg);
    }
  }
  for (i = 0; i < in_registered; i++) {
    if (in[i].initialized) {
      if (IN_PLUGIN(i)->stop)
        IN_PLUGIN(i)->stop();
      in[i].plugin->close(in[i].arg);
    }
  }
  rng_destroy(&midi_in);
}

void midi_reset(void)
{
}

void midi_stop(void)
{
  int i;
  for (i = 0; i < out_registered; i++)
    if (OUT_PLUGIN(i)->stop && out[i].initialized)
      OUT_PLUGIN(i)->stop();
  for (i = 0; i < in_registered; i++)
    if (IN_PLUGIN(i)->stop && in[i].initialized)
      IN_PLUGIN(i)->stop();
}

void midi_timer(void)
{
  int i;
  for (i = 0; i < out_registered; i++)
    if (OUT_PLUGIN(i)->run && out[i].initialized)
      OUT_PLUGIN(i)->run();
}

void midi_put_data(unsigned char *buf, size_t size)
{
  rng_add(&midi_in, size, buf);

  run_new_sb();
}

int midi_get_data_byte(unsigned char *buf)
{
  if (!rng_count(&midi_in))
    return 0;
  return rng_get(&midi_in, buf);
}

int midi_register_output_plugin(const struct midi_out_plugin *plugin)
{
  int index;
  if (out_registered >= MAX_OUT_PLUGINS) {
    error("Cannot register midi plugin %s\n", plugin->name);
    return 0;
  }
  index = out_registered++;
  out[index].plugin = plugin;
  out[index].id = PCM_ID_P;
  out[index].initialized = 0;
  return 1;
}

int midi_register_input_plugin(const struct midi_in_plugin *plugin)
{
  int index;
  if (in_registered >= MAX_IN_PLUGINS) {
    error("Cannot register midi plugin %s\n", plugin->name);
    return 0;
  }
  index = in_registered++;
  in[index].plugin = plugin;
  in[index].id = PCM_ID_R;
  in[index].initialized = 0;
  return 1;
}
