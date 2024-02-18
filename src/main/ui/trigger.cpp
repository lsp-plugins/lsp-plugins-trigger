/*
 * Copyright (C) 2024 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2024 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-trigger
 * Created on: 31 июл. 2021 г.
 *
 * lsp-plugins-trigger is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-trigger is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-trigger. If not, see <https://www.gnu.org/licenses/>.
 */

#include <private/plugins/trigger.h>
#include <private/ui/trigger.h>
#include <lsp-plug.in/plug-fw/ui.h>
#include <private/ui/trigger_midi.h>

namespace lsp
{
    namespace plugui
    {
        //---------------------------------------------------------------------
        // Plugin UI factory
        static const meta::plugin_t *plugin_uis[] =
        {
            &meta::trigger_mono,
            &meta::trigger_stereo,
            &meta::trigger_midi_mono,
            &meta::trigger_midi_stereo
        };

        static ui::Module *trigger_factory_func(const meta::plugin_t *meta)
        {
            return new trigger(meta);
        }

        static ui::Factory factory(trigger_factory_func, plugin_uis, 4);

        //---------------------------------------------------------------------
        // Trigger UI implementation
        trigger::trigger(const meta::plugin_t *meta): ui::Module(meta)
        {
        }

        trigger::~trigger()
        {
        }

        status_t trigger::init(ui::IWrapper *wrapper, tk::Display *dpy)
        {
            status_t res = ui::Module::init(wrapper, dpy);
            if (res != STATUS_OK)
                return res;

            // Seek for all velocity ports and create proxy ports
            for (size_t i=0, n=wrapper->ports(); i<n; ++i)
            {
                ui::IPort *port = wrapper->port(i);
                if (port == NULL)
                    continue;
                const meta::port_t *meta = port->metadata();
                if ((meta == NULL) || (meta->id == NULL))
                    continue;
                if (strstr(meta->id, "vl_") != meta->id)
                    continue;

                // Create proxy port
                trigger_midi::MidiVelocityPort *velocity = new trigger_midi::MidiVelocityPort();
                if (velocity == NULL)
                    return STATUS_NO_MEM;
                if ((res = velocity->init("midivel", port)) != STATUS_OK)
                    return res;
                if ((res = pWrapper->bind_custom_port(velocity)) != STATUS_OK)
                {
                    delete velocity;
                    return res;
                }
            }

            return STATUS_OK;
        }
    } // namespace plugui
} // namespace lsp
