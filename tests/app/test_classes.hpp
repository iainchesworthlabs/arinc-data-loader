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
    void configuration_model_replaces_its_rows();
    void load_model_describes_a_load();
    void load_model_keeps_the_operators_ticks();
    void load_model_shows_the_progress_of_the_ticked_loads();
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
    void a_target_needs_an_address_and_a_name();
    void abort_before_the_target_accepts_ends_the_operation();
    void a_second_job_is_refused_while_one_runs();
    void a_failing_host_ends_the_job_with_the_reason();
    void media_set_is_read_from_a_directory();
    void media_set_that_is_not_there_is_reported();
    void destroying_the_model_waits_for_the_engine();
    void upload_sends_the_ticked_loads();
    void an_upload_needs_a_target_a_media_set_and_a_tick();
    void a_load_the_target_cannot_read_ends_the_upload();
    void abort_before_the_target_accepts_ends_the_upload();
};
