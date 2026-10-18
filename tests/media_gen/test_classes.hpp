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
    void places_data_files_over_the_members();
    void cancel_writes_nothing();
    void writes_the_batch_when_asked();
};
