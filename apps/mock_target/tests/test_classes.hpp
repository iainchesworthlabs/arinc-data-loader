#pragma once

#include <QObject>

// The model: MockTargetModel on its worker thread, against the loader's engine over the fake
// platform (so no real network is touched and no real time passes).
class MockModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void start_stop_runs_the_engine();
    void an_upload_through_the_model_stores_the_files();
    void a_failing_link_reports_the_error();
    void loopback_only_binds_the_link_to_loopback();
    void a_name_the_loader_sends_is_stored_only_if_it_is_plain_and_portable();
    void a_load_directory_is_never_a_device_or_a_dotted_name();
    void starting_and_stopping_in_quick_succession_leaves_the_worker_free();
    void the_activity_roles_are_the_ones_the_log_view_reads();
};
