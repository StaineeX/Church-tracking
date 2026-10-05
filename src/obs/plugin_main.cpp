// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "obs/church_director_filter.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("church-director", "en-US")

bool obs_module_load(void)
{
    obs_register_source(&church_director_filter_info());
    return true;
}
