#pragma once

// The generator's tests, one class. Each test is a slot; main.cpp runs them all.

#include <QObject>

class GeneratorTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void header_file_name_follows_the_load_part_number();
    void generates_a_verified_media_set();
    void refuses_an_invalid_form();
    void refuses_duplicate_file_names();
    void refuses_a_name_the_folder_tree_would_refuse();
    void refuses_a_header_or_batch_file_name_the_folder_tree_would_refuse();
    void a_change_of_language_keeps_the_outcome_of_the_run();
    void any_check_value_follows_the_check_value_column();
    void places_data_files_over_the_members();
    void cancel_writes_nothing();
    void writes_the_batch_when_asked();
    void a_cancel_before_anything_is_written_says_nothing_was_written();
    void a_cancel_after_the_tree_is_written_removes_it_and_says_so();
    void a_cancel_after_the_set_verified_removes_it_too();
    void a_write_that_fails_leaves_no_partial_set();
    void a_file_that_cannot_be_allocated_ends_the_job_cleanly();
    void any_other_allocation_failure_ends_the_job_cleanly();
    void the_set_appears_under_its_name_in_one_step();
    void a_set_that_exists_is_never_overwritten();
};
