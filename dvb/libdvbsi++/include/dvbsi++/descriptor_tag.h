/*
 * $Id: descriptor_tag.h,v 1.5 2005/11/13 17:40:12 mws Exp $
 *
 * Copyright (C) 2002-2005 Andreas Oberritter <obi@saftware.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 *
 * See the file 'COPYING' in the top level directory for details.
 */

#ifndef __descriptor_tag_h__
#define __descriptor_tag_h__

enum SiDescriptorTag {
	/* 0x00 - 0x3F: ITU-T Rec. H.222.0 | ISO/IEC 13818-1 */
	VIDEO_STREAM_DESCRIPTOR				= 0x02,
	AUDIO_STREAM_DESCRIPTOR				= 0x03,
	HIERARCHY_DESCRIPTOR				= 0x04,
	REGISTRATION_DESCRIPTOR				= 0x05,
	DATA_STREAM_ALIGNMENT_DESCRIPTOR		= 0x06,
	TARGET_BACKGROUND_GRID_DESCRIPTOR		= 0x07,
	VIDEO_WINDOW_DESCRIPTOR				= 0x08,
	CA_DESCRIPTOR					= 0x09,
	ISO_639_LANGUAGE_DESCRIPTOR			= 0x0A,
	SYSTEM_CLOCK_DESCRIPTOR				= 0x0B,
	MULTIPLEX_BUFFER_UTILIZATION_DESCRIPTOR		= 0x0C,
	COPYRIGHT_DESCRIPTOR				= 0x0D,
	MAXIMUM_BITRATE_DESCRIPTOR			= 0x0E,
	PRIVATE_DATA_INDICATOR_DESCRIPTOR		= 0x0F,
	SMOOTHING_BUFFER_DESCRIPTOR			= 0x10,
	STD_DESCRIPTOR					= 0x11,
	IBP_DESCRIPTOR					= 0x12,
	CAROUSEL_IDENTIFIER_DESCRIPTOR			= 0x13,
	/* 0x40 - 0x7F: ETSI EN 300 468 V1.5.1 (2003-05) */
	NETWORK_NAME_DESCRIPTOR				= 0x40,
	SERVICE_LIST_DESCRIPTOR				= 0x41,
	STUFFING_DESCRIPTOR				= 0x42,
	SATELLITE_DELIVERY_SYSTEM_DESCRIPTOR		= 0x43,
	CABLE_DELIVERY_SYSTEM_DESCRIPTOR		= 0x44,
	VBI_DATA_DESCRIPTOR				= 0x45,
	VBI_TELETEXT_DESCRIPTOR				= 0x46,
	BOUQUET_NAME_DESCRIPTOR				= 0x47,
	SERVICE_DESCRIPTOR				= 0x48,
	COUNTRY_AVAILABILITY_DESCRIPTOR			= 0x49,
	LINKAGE_DESCRIPTOR				= 0x4A,
	NVOD_REFERENCE_DESCRIPTOR			= 0x4B,
	TIME_SHIFTED_SERVICE_DESCRIPTOR			= 0x4C,
	SHORT_EVENT_DESCRIPTOR				= 0x4D,
	EXTENDED_EVENT_DESCRIPTOR			= 0x4E,
	TIME_SHIFTED_EVENT_DESCRIPTOR			= 0x4F,
	COMPONENT_DESCRIPTOR				= 0x50,
	MOSAIC_DESCRIPTOR				= 0x51,
	STREAM_IDENTIFIER_DESCRIPTOR			= 0x52,
	CA_IDENTIFIER_DESCRIPTOR			= 0x53,
	CONTENT_DESCRIPTOR				= 0x54,
	PARENTAL_RATING_DESCRIPTOR			= 0x55,
	TELETEXT_DESCRIPTOR				= 0x56,
	TELEPHONE_DESCRIPTOR				= 0x57,
	LOCAL_TIME_OFFSET_DESCRIPTOR			= 0x58,
	SUBTITLING_DESCRIPTOR				= 0x59,
	TERRESTRIAL_DELIVERY_SYSTEM_DESCRIPTOR		= 0x5A,
	MULTILINGUAL_NETWORK_NAME_DESCRIPTOR		= 0x5B,
	MULTILINGUAL_BOUQUET_NAME_DESCRIPTOR		= 0x5C,
	MULTILINGUAL_SERVICE_NAME_DESCRIPTOR		= 0x5D,
	MULTILINGUAL_COMPONENT_DESCRIPTOR		= 0x5E,
	PRIVATE_DATA_SPECIFIER_DESCRIPTOR		= 0x5F,
	SERVICE_MOVE_DESCRIPTOR				= 0x60,
	SHORT_SMOOTHING_BUFFER_DESCRIPTOR		= 0x61,
	FREQUENCY_LIST_DESCRIPTOR			= 0x62,
	PARTIAL_TRANSPORT_STREAM_DESCRIPTOR		= 0x63,
	DATA_BROADCAST_DESCRIPTOR			= 0x64,
	/* 0x65: Reserved */
	DATA_BROADCAST_ID_DESCRIPTOR			= 0x66,
	TRANSPORT_STREAM_DESCRIPTOR			= 0x67,
	DSNG_DESCRIPTOR					= 0x68,
	PDC_DESCRIPTOR					= 0x69,
	AC3_DESCRIPTOR					= 0x6A,
	ANCILLARY_DATA_DESCRIPTOR			= 0x6B,
	CELL_LIST_DESCRIPTOR				= 0x6C,
	CELL_FREQUENCY_LINK_DESCRIPTOR			= 0x6D,
	ANNOUNCEMENT_SUPPORT_DESCRIPTOR			= 0x6E,
	APPLICATION_SIGNALLING_DESCRIPTOR		= 0x6F,
	ADAPTATION_FIELD_DATA_DESCRIPTOR		= 0x70,
	SERVICE_IDENTIFIER_DESCRIPTOR			= 0x71,
	SERVICE_AVAILABILITY_DESCRIPTOR			= 0x72,
	// TS 102 323
	DEFAULT_AUTHORITY_DESCRIPTOR			= 0x73,
	RELATED_CONTENT_DESCRIPTOR			= 0x74,
	TVA_ID_DESCRIPTOR				= 0x75,
	CONTENT_IDENTIFIER_DESCRIPTOR			= 0x76,
	// EN 301 192
	TIME_SLICE_FEC_IDENTIFIER_DESCRIPTOR		= 0x77,
	ECM_REPETITION_RATE_DESCRIPTOR			= 0x78,
	// EN 300 468 1.7.1
	S2_SATELLITE_DELIVERY_SYSTEM_DESCRIPTOR		= 0x79,
	/* 0x80 - 0xFE: User defined */
	/* 0xFF: Forbidden */
	FORBIDDEN_DESCRIPTOR				= 0xFF
};

enum CarouselDescriptorTag {
	/* ETSI EN 301 192 V1.3.1 (2003-05) */
	/* 0x00: Reserved */
	TYPE_DESCRIPTOR					= 0x01,
	NAME_DESCRIPTOR					= 0x02,
	INFO_DESCRIPTOR					= 0x03,
	MODULE_LINK_DESCRIPTOR				= 0x04,
	CRC32_DESCRIPTOR				= 0x05,
	LOCATION_DESCRIPTOR				= 0x06,
	EST_DOWNLOAD_TIME_DESCRIPTOR			= 0x07,
	GROUP_LINK_DESCRIPTOR				= 0x08,
	COMPRESSED_MODULE_DESCRIPTOR			= 0x09,
	SUBGROUP_ASSOCIATION_DESCRIPTOR			= 0x0A,
	/* 0x0B - 0x6F: Reserved */
	/* ETSI TS 101 812 V1.2.1 (2003-06) */
	LABEL_DESCRIPTOR				= 0x70,
	CACHING_PRIORITY_DESCRIPTOR			= 0x71,
	CONTENT_TYPE_DESCRIPTOR				= 0x72,
	/* 0x5F: PRIVATE_DATA_SPECIFIER_DESCRIPTOR */
	/* 0x73 - 0x7F: Reserved */
	/* 0x80 - 0xFE: User defined */
	/* 0xFF: FORBIDDEN_DESCRIPTOR */
};

enum MhpDescriptorTag {
	/* ETSI TS 101 812 V1.2.1 (2003-06) */
	APPLICATION_DESCRIPTOR				= 0x00,
	APPLICATION_NAME_DESCRIPTOR			= 0x01,
	TRANSPORT_PROTOCOL_DESCRIPTOR			= 0x02,
	DVB_J_APPLICATION_DESCRIPTOR			= 0x03,
	DVB_J_APPLICATION_LOCATION_DESCRIPTOR		= 0x04,
	EXTERNAL_APPLICATION_AUTHORISATION_DESCRIPTOR	= 0x05,
	/* 0x06 - 0x07: Reserved */
	DVB_HTML_APPLICATION_DESCRIPTOR			= 0x08,
	DVB_HTML_APPLICATION_LOCATION_DESCRIPTOR	= 0x09,
	DVB_HTML_APPLICATION_BOUNDARY_DESCRIPTOR	= 0x0A,
	APPLICATION_ICONS_DESCRIPTOR			= 0x0B,
	PREFETCH_DESCRIPTOR				= 0x0C,
	DII_LOCATION_DESCRIPTOR				= 0x0D,
	DELEGATED_APPLICATION_DESCRIPTOR		= 0x0E,
	PLUGIN_DESCRIPTOR				= 0x0F,
	APPLICATION_STORAGE_DESCRIPTOR			= 0x10,
	IP_SIGNALING_DESCRIPTOR				= 0x11,
	/* 0x12 - 0x5E: Reserved */
	/* 0x5F: PRIVATE_DATA_SPECIFIER_DESCRIPTOR */
	/* 0x80 - 0xFE: User defined */
	/* 0xFF: FORBIDDEN_DESCRIPTOR */
};

#endif /* __descriptor_tag_h__ */
