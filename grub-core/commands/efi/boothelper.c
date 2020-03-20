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

#define BOOTDEV_BUFFER_SIZE 60
 
static grub_efi_device_path_t *
miray_efi_chomp_device_path(grub_efi_device_path_t *dp)
{
   grub_efi_device_path_t * cur = dp;

   if (GRUB_EFI_END_ENTIRE_DEVICE_PATH (cur))
      return 0;

   while (1)
   {
      grub_efi_device_path_t * tmp = GRUB_EFI_NEXT_DEVICE_PATH(cur);

      if (GRUB_EFI_END_ENTIRE_DEVICE_PATH(tmp))
      {
         cur->type      = (cur->type & 0x80) | 0x7f;
         cur->subtype   = GRUB_EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE;
         cur->length    = 4;

         return dp;
      }
      cur = tmp;
   }
}
 
static grub_err_t
miray_machine_bootdev(char * buffer, grub_size_t buffer_size)
{
   grub_efi_loaded_image_t *image = NULL;
   char *device = NULL;
 
   image = grub_efi_get_loaded_image (grub_efi_image_handle);
   if (image == NULL)
   {
      grub_dprintf("miray", "No Image\n");
      return -1;
   }

   
   device = grub_efidisk_get_device_name (image->device_handle);

   if (device == NULL && grub_efi_net_config != NULL)
   {
      char * path = NULL;
      
      grub_efi_net_config(image->device_handle, &device, &path);
      if (device != NULL)
      {
         grub_dprintf("miray", "NET DEVICE (%s), <%s>", device, path);    
      }
      
      grub_free(path);
   }



   if (device == NULL)
   {
      // Try to find a boot device in the path,
      // This works for cd image boot
      grub_efi_device_path_t *dp;
      grub_efi_device_path_t *subpath;
      grub_efi_device_path_t *dp_copy;
   
      dp = grub_efi_get_device_path (image->device_handle);
      if (dp == 0)
      {
      grub_dprintf("miray", "No device path");
      return 0;
      }
   
      dp_copy = grub_efi_duplicate_device_path(dp);
      subpath = dp_copy;
   
      while ((subpath = miray_efi_chomp_device_path(subpath)) != 0)
      {
         device = miray_efi_get_diskname_from_path(subpath);
         if (device != 0) break;
      }
   
      grub_free(dp_copy);      
   }
   
   if (device == NULL)
   {
      grub_dprintf("miray", "No device");
      return -1;
   }
   
   grub_snprintf(buffer, buffer_size, "(%s)", device);
   
   grub_free (device);
   
   return GRUB_ERR_NONE;
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
   
   char buffer[BOOTDEV_BUFFER_SIZE + 1];

   if (state[0].set)
      env_key = state[0].arg;

   grub_err_t ret = miray_machine_bootdev(buffer, BOOTDEV_BUFFER_SIZE);
   if (ret != GRUB_ERR_NONE)
      return ret;

   grub_env_set(env_key, buffer);

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