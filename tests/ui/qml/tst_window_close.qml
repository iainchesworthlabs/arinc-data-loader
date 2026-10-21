import QtQuick
import QtQuick.Controls
import QtTest

// Closing the loader's window. With a job running the window asks first: "Keep working" leaves
// everything as it was; "Abort and close" asks the target to stop and closes when the job has
// ended - or at once, when the job ended while the question was open (no busy change follows
// then, so the window has to notice by itself). Every test makes a window of its own, because
// closing one is the point.
TestCase {
    id: testCase

    name: "WindowClose"
    when: windowShown
    width: 1280
    height: 900

    property var closing: null

    function newWindow() {
        const component = Qt.createComponent(mainQmlUrl)
        verify(component.status === Component.Ready, component.errorString())
        const made = component.createObject(null, { "app": appModel })
        verify(made !== null, "Main.qml did not make a window")
        made.width = 1400
        made.height = 1600
        waitForRendering(made.contentItem)
        wait(50)
        closing = made
        return made
    }

    function find(root, name) {
        const item = harness.findItem(root, name)
        verify(item !== null, "there is no item called " + name)
        return item
    }

    // Starts an Information operation that is certainly still running: the engine is held before
    // it takes the host.
    function startAHeldJob() {
        appModel.targetAddress = "10.0.0.2"
        appModel.thwIdPos = "HNPFMS_L"
        harness.holdNextJob()
        appModel.readInformation()
        tryVerify(function() { return appModel.busy }, 10000)
    }

    function initTestCase() {
        verify(harness.ready(), "the media set could not be written")
        appModel.completeFirstRun()
    }

    function init() {
        harness.releaseJob()
        harness.resetTarget()
        appModel.dismissError()
        tryVerify(function() { return !appModel.busy }, 20000)
    }

    function cleanup() {
        harness.releaseJob()
        tryVerify(function() { return !appModel.busy }, 20000)
        if (closing) {
            closing.destroy()
            closing = null
        }
    }

    function test_01_an_idle_window_closes_without_asking() {
        const window = newWindow()
        const dialog = find(window, "closeDialog")
        window.close()
        tryVerify(function() { return !window.visible })
        verify(!dialog.visible)
    }

    function test_02_keep_working_leaves_the_job_and_the_window_alone() {
        const window = newWindow()
        const dialog = find(window, "closeDialog")
        startAHeldJob()

        window.close()
        tryVerify(function() { return dialog.visible })
        verify(window.visible, "the window closed under a running job")

        const keep = dialog.standardButton(Dialog.Cancel)
        verify(keep !== null)
        compare(keep.text, "Keep working")
        mouseClick(keep)
        tryVerify(function() { return !dialog.visible })
        verify(window.visible)
        verify(appModel.busy)
        verify(appModel.canAbort, "the job was aborted by Keep working")

        // The job runs on to its end, and the window is still there.
        harness.releaseJob()
        tryCompare(appModel, "resultState", 1, 20000)
        tryVerify(function() { return !appModel.busy }, 20000)
        compare(appModel.resultText, "completed")
        verify(window.visible)
    }

    function test_03_stop_and_close_aborts_the_job_and_closes_when_it_has_ended() {
        const window = newWindow()
        const dialog = find(window, "closeDialog")
        startAHeldJob()

        window.close()
        tryVerify(function() { return dialog.visible })
        const stop = dialog.standardButton(Dialog.Ok)
        verify(stop !== null)
        compare(stop.text, "Abort and close")
        mouseClick(stop)

        // The abort was asked for; the window waits for the job to end before it goes.
        tryVerify(function() { return !appModel.canAbort })
        verify(appModel.busy)
        verify(window.visible, "the window did not wait for the job")
        harness.releaseJob()
        tryCompare(appModel, "resultState", 2, 20000)
        tryVerify(function() { return !appModel.busy }, 20000)
        compare(appModel.resultText, "aborted by the operator")
        tryVerify(function() { return !window.visible }, 10000)
    }

    // The job ended while the question was open: pressing "Abort and close" has nothing to stop,
    // and no change of the job's state is left to close the window - it has to close at once.
    function test_04_stop_and_close_after_the_job_ended_closes_at_once() {
        const window = newWindow()
        const dialog = find(window, "closeDialog")
        startAHeldJob()

        window.close()
        tryVerify(function() { return dialog.visible })
        harness.releaseJob()
        tryCompare(appModel, "resultState", 1, 20000)
        tryVerify(function() { return !appModel.busy }, 20000)
        verify(window.visible)
        verify(dialog.visible, "the question went away by itself")

        mouseClick(dialog.standardButton(Dialog.Ok))
        tryVerify(function() { return !window.visible }, 5000)
        verify(!dialog.visible)
        // Nothing was aborted: the job had finished on its own.
        compare(appModel.resultText, "completed")
        compare(appModel.resultState, 1)
    }
}
