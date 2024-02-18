/*
 * Copyright (C) 2024 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2024 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-trigger
 * Created on: 18 февр. 2024 г.
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

#ifndef PRIVATE_UI_TRIGGER_H_
#define PRIVATE_UI_TRIGGER_H_

#include <lsp-plug.in/plug-fw/ui.h>
#include <lsp-plug.in/fmt/Hydrogen.h>

#include <private/ui/sfz.h>

namespace lsp
{
    namespace plugui
    {
        class trigger: public ui::Module
        {
            public:
                explicit trigger(const meta::plugin_t *meta);
                virtual ~trigger() override;

                virtual status_t    init(ui::IWrapper *wrapper, tk::Display *dpy) override;
        };
    } /* namespace plugui */
} /* namespace lsp */



#endif /* PRIVATE_UI_TRIGGER_H_ */
