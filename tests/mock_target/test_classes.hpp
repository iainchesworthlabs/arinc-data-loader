#pragma once

#include <QObject>

// The model: MockTargetModel on its worker thread, against the loader's engine over the fake
// platform (so no real network is touched and no real time passes).
class MockModelTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void start_stop_runs_the_engine();
    void an_upload_through_the_model_stores_the_files();
    void a_failing_link_reports_the_error();
    void loopback_only_binds_the_link_to_loopback();
};
