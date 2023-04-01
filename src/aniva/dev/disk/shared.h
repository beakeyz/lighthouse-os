#ifndef __ANIVA_SHARED_DISK_DEFS__
#define __ANIVA_SHARED_DISK_DEFS__
#include <libk/stddef.h>

// TODO: found an elaborate version of this structure
// in the microsoft driver documentation, let's yoink some
// of that =)
// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ata/ns-ata-_identify_device_data
// hopefully they don't mind

typedef struct {
  struct {
    uint16_t res1 : 1;
    uint16_t retired3 : 1;
    uint16_t response_incomplete : 1;
    uint16_t retired2 : 3;
    uint16_t fixed_device : 1;
    uint16_t removable_media : 1;
    uint16_t retired1 : 7;
    uint16_t dev_type : 1;
  } general_config;

  uint16_t obsolete;
  uint16_t specific_config;

  uint16_t obsolete2;
  uint16_t retired[2];
  uint16_t obsolete3;

  uint16_t reserved_for_cfa[2];
  uint16_t retired2;
  uint8_t serial_number[20];

  uint16_t retired3[2];
  uint16_t obsolete4;

  uint16_t firmware_revision[4];
  uint8_t model_number[40];

  uint16_t maximum_logical_sectors_per_drq;
  uint16_t trusted_computing_features;
  uint16_t capabilities[2];
  uint16_t obsolete5[2];
  uint16_t validity_flags;
  uint16_t obsolete6[5];

  uint16_t security_features;

  uint32_t max_28_bit_addressable_logical_sector;
  uint16_t obsolete7;
  uint16_t dma_modes;
  uint16_t pio_modes;

  uint16_t minimum_multiword_dma_transfer_cycle;
  uint16_t recommended_multiword_dma_transfer_cycle;

  uint16_t minimum_multiword_pio_transfer_cycle_without_flow_control;
  uint16_t minimum_multiword_pio_transfer_cycle_with_flow_control;

  uint16_t additional_supported;
  uint16_t reserved3[5];
  uint16_t queue_depth;

  uint16_t serial_ata_capabilities;
  uint16_t serial_ata_additional_capabilities;
  uint16_t serial_ata_features_supported;
  uint16_t serial_ata_features_enabled;
  uint16_t major_version_number;
  uint16_t minor_version_number;
  uint16_t commands_and_feature_sets_supported[3];
  uint16_t commands_and_feature_sets_supported_or_enabled[3];
  uint16_t ultra_dma_modes;

  uint16_t timing_for_security_features[2];
  uint16_t apm_level;
  uint16_t master_password_id;

  uint16_t hardware_reset_results;
  uint16_t obsolete8;

  uint16_t stream_minimum_request_time;
  uint16_t streaming_transfer_time_for_dma;
  uint16_t streaming_access_latency;
  uint16_t streaming_performance_granularity[2];

  uint64_t user_addressable_logical_sectors_count;

  uint16_t streaming_transfer_time_for_pio;
  uint16_t max_512_byte_blocks_per_data_set_management_command;
  uint16_t physical_sector_size_to_logical_sector_size;
  uint16_t inter_seek_delay_for_acoustic_testing;
  uint16_t world_wide_name[4];
  uint16_t reserved4[4];
  uint16_t obsolete9;

  uint32_t logical_sector_size;

  uint16_t commands_and_feature_sets_supported2;
  uint16_t commands_and_feature_sets_supported_or_enabled2;

  uint16_t reserved_for_expanded_supported_and_enabled_settings[6];
  uint16_t obsolete10;

  uint16_t security_status;
  uint16_t vendor_specific[31];
  uint16_t reserved_for_cfa2[8];
  uint16_t device_nominal_form_factor;
  uint16_t data_set_management_command_support;
  uint16_t additional_product_id[4];
  uint16_t reserved5[2];
  uint16_t current_media_serial_number[30];
  uint16_t sct_command_transport;
  uint16_t reserved6[2];

  uint16_t logical_sectors_alignment_within_physical_sector;

  uint32_t write_read_verify_sector_mode_3_count;
  uint32_t write_read_verify_sector_mode_2_count;

  uint16_t obsolete11[3];
  uint16_t nominal_media_rotation_rate;
  uint16_t reserved7;
  uint16_t obsolete12;
  uint16_t write_read_verify_feature_set_current_mode;
  uint16_t reserved8;
  uint16_t transport_major_version_number;
  uint16_t transport_minor_version_number;
  uint16_t reserved9[6];

  uint64_t extended_user_addressable_logical_sectors_count;

  uint16_t minimum_512_byte_data_blocks_per_download_microcode_operation;
  uint16_t max_512_byte_data_blocks_per_download_microcode_operation;

  uint16_t reserved10[19];

  // hihi
  uint16_t signature : 8;
  uint16_t checksum : 8;
} __attribute__((packed)) ata_identify_block_t;

typedef uintptr_t disk_offset_t;

#endif // !__ANIVA_SHARED_DISK_DEFS__