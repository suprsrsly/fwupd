/*
 * Copyright (C) 2023 Canonical Ltd.
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN "FuFirmware"

#include "config.h"

#include "fu-bytes.h"
#include "fu-csv-firmware.h"
#include "fu-input-stream.h"
#include "fu-mem.h"
#include "fu-sbatlevel-section-struct.h"
#include "fu-sbatlevel-section.h"

G_DEFINE_TYPE(FuSbatlevelSection, fu_sbatlevel_section, FU_TYPE_FIRMWARE);

static gboolean
fu_sbatlevel_section_add_entry(FuFirmware *firmware,
			       GInputStream *stream,
			       gsize offset,
			       const gchar *entry_name,
			       guint64 entry_idx,
			       FwupdInstallFlags flags,
			       GError **error)
{
	gsize bufsz = 0;
	const guint8 *buf;
	gsize size = 0;
	g_autoptr(FuFirmware) entry_fw = NULL;
	g_autoptr(GBytes) entry_blob = NULL;
	g_autoptr(GBytes) fw = NULL;

	fw = fu_input_stream_read_bytes(stream, offset, G_MAXSIZE, error);
	if (fw == NULL)
		return FALSE;
	buf = g_bytes_get_data(fw, &bufsz);

	/* look for the null terminator */
	for (size = 0; ((offset + size) < bufsz); ++size) {
		if (buf[offset + size] == 0)
			break;
	}

	entry_fw = fu_csv_firmware_new();
	fu_csv_firmware_add_column_id(FU_CSV_FIRMWARE(entry_fw), "$id");
	fu_csv_firmware_add_column_id(FU_CSV_FIRMWARE(entry_fw), "component_generation");
	fu_csv_firmware_add_column_id(FU_CSV_FIRMWARE(entry_fw), "date_stamp");

	fu_firmware_set_idx(entry_fw, entry_idx);
	fu_firmware_set_id(entry_fw, entry_name);
	fu_firmware_set_offset(entry_fw, offset);
	entry_blob = fu_bytes_new_offset(fw, offset, size, error);
	if (entry_blob == NULL)
		return FALSE;
	if (!fu_firmware_add_image_full(firmware, entry_fw, error))
		return FALSE;
	if (!fu_firmware_parse(entry_fw, entry_blob, flags, error)) {
		g_prefix_error(error, "failed to parse %s: ", entry_name);
		return FALSE;
	}

	return TRUE;
}

static gboolean
fu_sbatlevel_section_parse(FuFirmware *firmware,
			   GInputStream *stream,
			   gsize offset,
			   FwupdInstallFlags flags,
			   GError **error)
{
	gsize header_offset = offset + FU_STRUCT_SBAT_LEVEL_SECTION_HEADER_OFFSET_PREVIOUS;
	guint32 previous_addr;
	guint32 latest_addr;
	g_autoptr(GByteArray) st = NULL;

	st = fu_struct_sbat_level_section_header_parse_stream(stream, offset, error);
	if (st == NULL)
		return FALSE;

	previous_addr = fu_struct_sbat_level_section_header_get_previous(st);

	if (!fu_sbatlevel_section_add_entry(firmware,
					    stream,
					    header_offset + previous_addr,
					    "previous",
					    0,
					    flags,
					    error))
		return FALSE;

	latest_addr = fu_struct_sbat_level_section_header_get_latest(st);

	if (!fu_sbatlevel_section_add_entry(firmware,
					    stream,
					    header_offset + latest_addr,
					    "latest",
					    1,
					    flags,
					    error))
		return FALSE;

	return TRUE;
}

static void
fu_sbatlevel_section_init(FuSbatlevelSection *self)
{
	fu_firmware_set_images_max(FU_FIRMWARE(self), 2);
}

static void
fu_sbatlevel_section_class_init(FuSbatlevelSectionClass *klass)
{
	FuFirmwareClass *firmware_class = FU_FIRMWARE_CLASS(klass);

	firmware_class->parse = fu_sbatlevel_section_parse;
}

FuFirmware *
fu_sbatlevel_section_new(void)
{
	return FU_FIRMWARE(g_object_new(FU_TYPE_SBATLEVEL_SECTION, NULL));
}
