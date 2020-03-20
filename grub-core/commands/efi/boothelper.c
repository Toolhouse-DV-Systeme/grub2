/*
 *  Extension for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010-2020 Miray Software <oss@miray.de>
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/command.h>
#include <grub/err.h>
#include <grub/types.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/extcmd.h>
#include <grub/env.h>

#include <grub/efi/efi.h>
#include <grub/efi/disk.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_efi_device_path_t *
chomp_device_path(grub_efi_device_path_t *dp)
{
   grub_efi_device_path_t * last = grub_efi_find_last_device_path(dp);
   if (!last)
      return NULL;

   last->type      = (last->type & 0x80) | 0x7f;
   last->subtype   = GRUB_EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE;
   last->length    = 4;

   return dp;
}

static char *
miray_machine_bootdev(void)
{
   char * ret = NULL;
   grub_efi_loaded_image_t *image = NULL;
   char *device = NULL;

   image = grub_efi_get_loaded_image (grub_efi_image_handle);
   if (image == NULL)
   {
      grub_dprintf("bootdev", "No Image\n");
      grub_error(GRUB_ERR_UNKNOWN_DEVICE, "No loaded image");
      return NULL;
   }

   device = grub_efidisk_get_device_name (image->device_handle);

   if (device == NULL && grub_efi_net_config != NULL)
   {
      char * path = NULL;

      grub_efi_net_config(image->device_handle, &device, &path);
      if (device != NULL)
      {
         grub_dprintf("bootdev", "NET DEVICE (%s), <%s>", device, path);
      }

      grub_free(path);
   }

   if (device == NULL)
   {
      // Try to find a boot device in the path,
      // This works for cd image boot
      grub_efi_device_path_t *dp;
      grub_efi_device_path_t *dp_copy;

      dp = grub_efi_get_device_path (image->device_handle);
      if (!dp)
      {
        grub_dprintf("bootdev", "No device path");
        grub_error(GRUB_ERR_UNKNOWN_DEVICE, "No device path found");
        return NULL;
      }

      dp_copy = grub_efi_duplicate_device_path(dp);
      if (dp_copy)
      {
         grub_efi_device_path_t * subpath = dp_copy;

         while ((subpath = chomp_device_path(subpath)) != 0)
         {
            device = miray_efi_get_diskname_from_path(subpath);
            if (device != 0) break;
         }

         grub_free(dp_copy);
      }
   }

   if (device == NULL)
   {
      grub_dprintf("bootdev", "No device");
      grub_error(GRUB_ERR_UNKNOWN_DEVICE, "Could not find boot device");
      return NULL;
   }

   ret = grub_xasprintf("(%s)", device);

   grub_free (device);

   return ret;
}


static const struct grub_arg_option options_bootdev[] =
{
   {"set",             's', 0,
      ("Set a variable to return value."), "VAR", ARG_TYPE_STRING},
};

static grub_err_t
miray_cmd_bootdev (grub_extcmd_context_t ctxt,
                   int argc __attribute__ ((unused)),
                   char **args __attribute__ ((unused)))
{
   struct grub_arg_list *state = ctxt->state;
   const char * env_key = "bootdev";
   char * dev = NULL;

   if (state[0].set)
      env_key = state[0].arg;

   dev = miray_machine_bootdev();
   if (!dev)
      return grub_errno;

   grub_env_set(env_key, dev);

   grub_free(dev);

   return GRUB_ERR_NONE;
}


static grub_extcmd_t cmd_bootdev;

GRUB_MOD_INIT(boothelper)
{
   cmd_bootdev = grub_register_extcmd ("bootdev", miray_cmd_bootdev,
                                       0, 0, N_("Set bootdev variable"), options_bootdev);
}

GRUB_MOD_FINI(boothelper)
{
   grub_unregister_extcmd(cmd_bootdev);
}
