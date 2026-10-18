#pragma once

#include <QObject>

// Plain functions of the application's core.
class AddressTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void parses_four_octets();
    void refuses_everything_else();
    void formats_what_it_parses();
    void target_name_is_hardware_id_and_position();
};

// What the engine's notices say.
class NoticeTextTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void names_the_file_and_gives_the_number_its_unit();
    void says_how_a_failed_transfer_ended();
    void leaves_the_rest_as_they_are();
};

// The lists the window shows.
class ModelsTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void target_model_exposes_what_find_found();
    void event_model_keeps_the_newest_lines();
    void event_model_capacity_is_per_instance();
    void configuration_model_replaces_its_rows();
    void load_model_describes_a_load();
    void load_model_keeps_the_operators_ticks();
    void load_model_keeps_ticks_for_loads_that_are_still_there();
    void load_model_shows_the_progress_of_the_ticked_loads();
    void file_model_lists_the_files_of_the_set();
    void download_model_holds_the_operators_names();
    void download_model_ticks_the_targets_list_and_keeps_the_choice();
};

// The operator's controls of a running job (the runner's controller): the abort and the
// deferred choice of files.
class ControlsTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void a_request_and_a_choice_are_posted_and_forgotten();
};

// The operator's preferences in QSettings.
class SettingsTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void the_defaults_hold_without_a_stored_value();
    void what_is_written_is_read_back();
    void numbers_out_of_range_are_kept_as_they_were();
};

// Finding the file the target asks for in a media set on disk.
class MediaSourceTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void index_maps_the_files_to_their_directories();
    void index_refuses_a_pathname_a_tree_cannot_hold();
    void two_files_of_one_name_are_told_apart_by_their_crc();
    void one_file_of_a_name_is_served_whatever_the_crc();
    void no_such_file_is_not_found();
    void reads_at_offsets_and_gives_back_its_slots();
};

// Whole jobs through LoaderModel: the engine on its own thread, against the simulated target on
// the fake platform (so no real network is touched and no real time passes).
class LoaderModelTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void information_reads_the_configuration();
    void find_lists_the_targets();
    void selecting_a_target_fills_the_address();
    void selecting_a_target_exposes_its_properties();
    void a_target_needs_an_address_and_a_name();
    void clearing_a_target_field_restores_its_default();
    void abort_before_the_target_accepts_ends_the_operation();
    void a_second_job_is_refused_while_one_runs();
    void a_failing_host_ends_the_job_with_the_reason();
    void a_failure_surfaces_for_the_dialog();
    void media_set_is_read_from_a_directory();
    void media_set_that_is_not_there_is_reported();
    void the_summary_lists_the_files_with_their_crcs();
    void destroying_the_model_waits_for_the_engine();
    void upload_sends_the_ticked_loads();
    void an_upload_needs_a_target_a_media_set_and_a_tick();
    void a_load_the_target_cannot_read_ends_the_upload();
    void abort_before_the_target_accepts_ends_the_upload();
    void a_set_is_uploadable_only_after_the_check_passes();
    void a_corrupted_set_fails_the_check_and_stays_refused();
    void check_media_set_revalidates_files_changed_on_disk();
    void findings_warn_but_do_not_block_the_upload();
    void with_validate_on_open_off_the_set_waits_for_a_check();
    void the_control_port_flows_into_the_loader();
    void auto_find_runs_before_an_upload_when_no_target_is_selected();
    void auto_find_with_several_targets_starts_nothing();
    void the_last_media_set_is_restored_after_startup();
    void a_missing_last_media_set_is_skipped_quietly();
    void the_log_capacity_preference_is_applied();
    void a_media_defined_download_writes_the_files_into_the_numbered_directory();
    void each_download_gets_the_next_numbered_directory();
    void an_operator_defined_download_takes_the_choice_through_the_gate();
    void choosing_none_of_the_targets_files_cancels_the_download();
    void a_download_with_a_missing_file_ends_as_a_failure();
    void abort_of_a_download_stops_it();
    void a_download_needs_a_target_a_directory_and_files();
    void a_download_refuses_a_name_that_is_not_a_safe_file_name();
    void a_second_job_is_refused_while_a_download_runs();
    void auto_find_runs_before_a_download_when_no_target_is_selected();
};
