# Scenarios for the mock target

A scenario is a file that says which fault the [mock target](../mock-target.md) shows, and when. It is for a test that has to be run again: the same file gives the same faults at the same moments, so a loader that failed on Tuesday can be shown the same failure on Wednesday. `adl_mock` takes one with the option `--scenario` ([command line](command-line.md#adl_mock)), and the [mock target's window](mock-target.md#a-scenario-file) takes the same file with its **Scenario file...** button.

A scenario is a list of steps. Each step has a trigger (the second request, the fourth status file, twenty seconds into an operation) and an action: any fault the mock target has, a delay, or a fault on the datagrams the target sends (lose one, send one twice, send one late, send two in the wrong order). A seed fixes everything that is left to chance, so that "lose half of the status files" loses the same ones every time.

The mock target is a simulated target, not a real one. A scenario makes it misbehave on purpose, in the ways listed below; it does not say what a real target does.

## An example

```json
{
  "kind": "adl-mock-scenario",
  "format": 1,
  "name": "Slow start, lost status files, then an abort",
  "description": "The first request waits 2 s. Uploads answer a little late. Half the status files from the 2nd on are lost, 3 times at most. 20 s into an operation the target gives up.",
  "seed": 20261120,
  "steps": [
    {
      "when": { "request": 1 },
      "do": { "action": "wait", "seconds": 2 }
    },
    {
      "comment": "only the second upload request, held back between 200 and 900 ms",
      "when": { "request": 2, "operation": "upload" },
      "do": { "action": "delay", "min_ms": 200, "max_ms": 900 }
    },
    {
      "when": { "status_file": 2, "every": 1, "times": 3 },
      "per_mille": 500,
      "do": { "action": "drop_status_file" }
    },
    {
      "when": { "after_ms": 20000 },
      "do": { "action": "abort" }
    }
  ]
}
```

Run with `adl_mock --scenario slow-start.json --json`, it answers the first request with WAIT for two seconds. The data loader asks again, and the answer to the second upload request comes a drawn number of milliseconds late. Of the status files of the upload, from the second on, each is lost with a chance of one in two, three times at most. Twenty seconds after an operation began, the target aborts it.

A second example is about the datagrams. Its steps lose some of the blocks of a status file, send an acknowledgement three times, hold datagrams back so that a later one goes first, and delay the datagrams of one file:

```json
{
  "kind": "adl-mock-scenario",
  "format": 1,
  "name": "A line that loses data",
  "description": "One in ten blocks of a status file is lost. The acknowledgement of block 3 of an upload is sent three times. After the first status file, the next two datagrams are held back until another has gone. From half a second into an operation, 20 datagrams of LOAD1.BIN are late by 100 to 300 ms.",
  "seed": 20261121,
  "steps": [
    {
      "when": { "request": 1, "every": 1 },
      "on": { "datagram": "data", "file": ".LUS" },
      "per_mille": 100,
      "count": 65535,
      "do": { "action": "drop_datagram" }
    },
    {
      "when": { "request": 1, "operation": "upload" },
      "on": { "datagram": "ack", "block": 3 },
      "do": { "action": "duplicate_datagram", "copies": 3 }
    },
    {
      "when": { "status_file": 1 },
      "on": {},
      "count": 2,
      "do": { "action": "reorder_datagram", "within_ms": 400 }
    },
    {
      "when": { "after_ms": 500 },
      "on": { "file": "LOAD1.BIN" },
      "count": 20,
      "do": { "action": "delay", "min_ms": 100, "max_ms": 300 }
    }
  ]
}
```

The steps with `on` and `count` are explained in [the datagrams](#on-and-count-the-datagrams-a-step-acts-on).

## The file

The file is JSON in UTF-8, at most 262144 bytes. It is read strictly: no comment, no trailing comma, no key given twice, no text that is not UTF-8, at most 8 levels of nesting. Numbers are whole numbers; a number with a fraction or an exponent is refused where a number is wanted. A key that this version does not know is refused and not ignored, so that a mistyped key cannot be a fault that never happens.

Other scenario files of the same family have the same top level and the same shape of step: `kind` says whom the file is for, and `format` which version of the file it is.

| Key | Value | Limits |
| --- | --- | --- |
| `kind` | `adl-mock-scenario` | Required. A scenario for a loader has the kind `adl-loader-scenario` and is refused here, with a sentence that says so |
| `format` | 1 | Required. A larger number is refused as made for a newer version |
| `name` | Text | Optional, 1 to 64 characters, no control character |
| `description` | Text | Optional, 1 to 1000 characters |
| `seed` | A whole number | Optional, 0 to 9007199254740991. Needed when a step uses `per_mille` or a range of delays |
| `steps` | A list of steps | Required, 0 to 1000 steps. An empty list, `"steps": []`, is a scenario that changes nothing |

The steps are counted from 1, in the order of the file. The number is what the sentences and the events say.

### A step

| Key | Value | Limits |
| --- | --- | --- |
| `when` | An object | Required: what makes the step happen, below |
| `do` | An object | Required: what it does, below |
| `per_mille` | A whole number | Optional, 1 to 1000. Each time the trigger occurs the step is applied with this chance in a thousand; for `drop_datagram`, `duplicate_datagram` and `reorder_datagram`, for each datagram that it acts on. Without it, always. Needs a `seed` |
| `on` | An object | Optional, for `delay`, `drop_datagram`, `duplicate_datagram` and `reorder_datagram` only: which of the target's datagrams the step acts on ([below](#on-and-count-the-datagrams-a-step-acts-on)) |
| `count` | A whole number | Optional, 1 to 65535, for the same four actions: how many datagrams the step acts on each time it fires. Without it, one |
| `comment` | Text | Optional, 1 to 200 characters. The program never uses it |

### `when`

`when` has exactly one of `request`, `status_file` and `after_ms`, and may add `every`, `times` and `operation`.

| Key | Value | Meaning |
| --- | --- | --- |
| `request` | 1 to 1000000 | The nth request for an initialization file that the target answers, counted from the start of the run |
| `status_file` | 1 to 1000000 | The nth status file of an operation, the first being the one that says "accepted". A status file that is lost still counts |
| `after_ms` | 0 to 86400000 | This many milliseconds after the operation began |
| `every` | 1 to 1000000; with `after_ms`, 10 to 86400000 | After the first time, again every this many requests, status files or milliseconds |
| `times` | 1 to 1000000 | At most this many times in all, in the whole run |
| `operation` | `information`, `upload`, `download_media_defined` or `download_operator_defined` | Only for that operation, and counting only requests for it |

A request counts when the target begins to answer it. A request that it answers with WAIT counts, and so does the data loader's repeat of it, and so does a request that is sent again and answered again. A request that arrives while an operation is running is not answered and does not count. A refused operation never began, so a step with `after_ms` never fires for it.

`request` counts from the start of the run. `status_file` and `after_ms` count from the start of each operation, so a step with one of them can fire in every operation; `times` limits that to a number in the whole run.

When several steps fire at the same moment they act in the order of the file. A `wait` answers the request, so a `refuse`, an `invalid_init_file` and a `corrupt_served` for the same request do nothing. An `abort` wins over a `drop_status_file` for the same status file. A status file that ends an operation is never lost: a `drop_status_file` for it is told as applied, and does nothing.

### `on` and `count`: the datagrams a step acts on

The actions `delay`, `drop_datagram`, `duplicate_datagram` and `reorder_datagram` act on datagrams: the ones the target sends. That is its requests for a file, its option acknowledgements, its acknowledgements of what the data loader sends, the blocks of the status files it sends, and its error packets. What the data loader sends is not touched.

When the trigger of such a step occurs, and its chance says yes, the step is armed: it acts on the datagrams that follow, up to `count` of them (one, if `count` is not given), and then it waits for its trigger to occur again, which arms it afresh with a new count. An operation that ends drops what is still armed. A datagram goes to the first armed step, in the order of the file, whose `on` it matches; the other steps stay armed for the next datagram.

`on` says which datagrams count. A step with no `on`, or with `"on": {}`, acts on the next datagram whatever it is. The keys of `on` narrow that down, and a datagram must match all that are given:

| Key | Value | Meaning |
| --- | --- | --- |
| `datagram` | `request`, `option_ack`, `ack`, `data`, `error` or `any` | The kind of datagram. `any` is the same as no key, and also takes the answers to a FIND, which no other word does |
| `file` | Text of 1 to 64 characters | The file of the transfer the datagram belongs to, as the target names it (`HNPFMS_L.LUS` for a status file, `APP.LUP` for a file of an upload). Upper and lower case are the same. A name that starts with a dot is an extension: `.LUS` is every file whose name ends in `.LUS` |
| `block` | A whole number, 0 to 65535 | The block number of an `ack` or a `data` datagram. With the kind `request`, `option_ack` or `error` it is refused. The acknowledgement of an option acknowledgement is block 0 |

A step with `request` is armed when the target begins to answer that request, so it can act on the answer. A step with `status_file` is armed when the target is about to send that status file, so it can act on the blocks of that file. A step with `after_ms` is armed at that time, and acts on the datagrams after it.

### `do`

`do` has `action` and the keys of that action, and no others.

| `action` | Keys | What it does | Goes with |
| --- | --- | --- | --- |
| `wait` | `seconds`, 1 to 65535 | Answers the request with WAIT for that many seconds. The data loader asks again, and that is the next request | `request` |
| `refuse` | `code`: `"0x1000"` (denied, the default) or `"0x1002"` (not supported) | Refuses the operation this request asks for | `request` |
| `invalid_init_file` | none | Serves an initialization file of an invalid structure | `request` |
| `drop_status_file` | none | Loses a status file: that one, or with `after_ms` the next one | `status_file`, `after_ms` |
| `abort` | none | The target aborts the operation (0x1003): in place of that status file, or with `after_ms` at once | `status_file`, `after_ms` |
| `silent` | none | The target says nothing more in this operation: not that status file, or with `after_ms` from then on. A new request wakes it | `status_file`, `after_ms` |
| `corrupt_served` | none | Flips the lowest bit of the middle byte of a file the target serves, and leaves the check value as it was for the file before, so the data loader's check fails. With `request` the file is the initialization file of that request; with `status_file` or `after_ms` it is the next file a download serves. In an upload or an Information operation the target serves no file, and there it does nothing | `request`, `status_file`, `after_ms` |
| `wrong_checksum` | none | Writes a wrong check value option: the next one it writes. This is the action for what the target itself says about a file; the data loader reports it as a check value that does not match and keeps the file | `status_file`, `after_ms` |
| `delay` | `ms`, 1 to 60000; or `min_ms` and `max_ms`, 0 to 60000, the least not above the most | Holds back the datagrams the step acts on by that long (the next one, or the next `count` that match `on`). With `min_ms` and `max_ms` the time is drawn from the seed, once, when the trigger occurs. The datagrams after it follow as usual. TFTP waits for each block, so the whole transfer is that much later | `request`, `status_file`, `after_ms` |
| `drop_datagram` | none | Does not send the datagram. Whoever is waiting for it sends again after its own time-out: the data loader sends a block again when its acknowledgement was lost, and the target sends a block of a status file again when it hears nothing | `request`, `status_file`, `after_ms` |
| `duplicate_datagram` | `copies`, 2 to 16, 2 if left out | Sends the datagram `copies` times in a row, so with 2 it is sent twice | `request`, `status_file`, `after_ms` |
| `reorder_datagram` | `within_ms`, 1 to 60000, 1000 if left out | Holds the datagram back and sends it straight after the next datagram that the target sends (one that is dropped or delayed does not count), or after `within_ms` if none has gone by then. What was sent first arrives second | `request`, `status_file`, `after_ms` |

More examples of `do`:

```json
{ "action": "refuse", "code": "0x1002" }
{ "action": "delay", "ms": 300 }
{ "action": "wait", "seconds": 5 }
{ "action": "silent" }
{ "action": "drop_datagram" }
{ "action": "duplicate_datagram", "copies": 3 }
{ "action": "reorder_datagram", "within_ms": 400 }
```

An action waits for the thing it acts on: the next status file, the next file served, the next check value written, the next datagram. One that finds nothing to act on when the operation ends is dropped. A step on datagrams that was armed and met no datagram that matched is told at the end of the run, as `scenario_idle`. A new action word in a later format is refused by this version, because running half a scenario would mislead; `adl_mock --scenario-actions` lists the words this version knows.

## The seed

Chance appears in two places only: `per_mille`, and a range of delays. Without either, the seed is never used. A scenario that uses one and has no `seed` is refused.

The numbers come from SplitMix64, written out in integer arithmetic only, so that every system and every standard library gives the same ones. The `seed` starts a generator, and its nth output starts the generator of step n. A step takes one number for `per_mille` (if it has one) and then one for a range of delays (if it has one) at each time its trigger occurs, whether or not the chance says yes, so that what a step draws depends on how many times it has occurred and on nothing else. The three actions that act on one datagram each, `drop_datagram`, `duplicate_datagram` and `reorder_datagram`, take no number when their trigger occurs. They take one for `per_mille`, from the same generator, for each datagram that matches `on`, so that `"per_mille": 100` loses about one datagram in ten of those it could, and the same ones for the same seed if the same datagrams are sent in the same order. A datagram that the chance spares is not counted against `count`. A number in a range is drawn by rejection, so that no number is likelier than another. A chance of `per_mille` applies when a draw from 1 to 1000 is at most that number.

The numbers are the same on every run, so the faults of a run are the same, if the loader does the same. Time is the exception: a step with `after_ms` is as steady as the clock and the loader are, and the order of two things that happen a few milliseconds apart can change from one run to the next.

## Running `adl_mock` with a scenario

`--scenario <file>` takes the faults from the file in place of the fault options. It cannot be given with `--wait-first`, `--wait-every`, `--refuse`, `--refuse-code`, `--invalid-init-file`, `--drop-status-every`, `--corrupt-served`, `--wrong-checksum`, `--abort-after` or `--silent`; the program says which. The identity, the files, the network, the pace, `--operations`, `--seconds` and `--json` can be given with it. A scenario with a step that goes silent needs `--seconds`, as `--silent` does, because the operation never ends.

The file is read before anything is opened. If it cannot be used the program writes one sentence and ends with exit code 5; nothing is written on the standard output.

`adl_mock --scenario-actions` writes the words that `action` may have in this version, one for each line, on the standard output, and ends with exit code 0. A script can use it to find out whether a scenario it has made can be run. A program from before the faults on a datagram does not know the option.

Every run, with a scenario or not, says the same in `started`: `scenario_features` is the list of those words, in the order of the table above. A program that has no such key knows none of the words `drop_datagram`, `duplicate_datagram` and `reorder_datagram`, and refuses a scenario that uses one, with exit code 5.

With `--scenario` the events change in four ways. Everything else is as in [the command line](command-line.md#what-it-writes).

- `started` has `"--scenario"` in `faults`, and the keys `scenario` (the path as it was given), `scenario_steps` (the number of steps) and `scenario_seed` (the seed, or `null`).
- Every time the trigger of a step occurs, a `scenario_step` event is written, before what the action causes.
- Every time a step on datagrams meets a datagram that matches it, a `scenario_datagram` event is written, before the datagram goes.
- `stopped` has `scenario_unused`: the numbers of the steps that never fired, and `scenario_idle`: the numbers of the steps on datagrams that fired and met no datagram that matched. A step that never fired is not an error; this is how a run that did not do what the file says can be seen.

A `scenario_step` has these keys beside `event` and `t`:

| Key | Value |
| --- | --- |
| `step` | The step's number in the file, from 1 |
| `trigger` | `request`, `status_file` or `after_ms` |
| `at` | The request number, the status file number, or the milliseconds the step was set for |
| `operation` | `information`, `upload`, `download_media_defined` or `download_operator_defined` |
| `action` | The action's word |
| `applied` | `true`, or `false` when `per_mille` said no |
| `seconds`, `status`, `ms` | Only for `wait` (the seconds), `refuse` (the status code as a number: 4096 or 4098) and `delay` (the milliseconds that were drawn, whether or not the chance said yes) |
| `copies`, `within_ms` | Only for `duplicate_datagram` and `reorder_datagram`: the number of copies, and the time in milliseconds |
| `count`, `datagram`, `file`, `block` | Only for the actions on datagrams: how many datagrams the step is armed for, and the keys of its `on` that it has, with `any` left out |

A `scenario_datagram` has these keys beside `event` and `t`:

| Key | Value |
| --- | --- |
| `step` | The step's number in the file, from 1 |
| `action` | `delay`, `drop_datagram`, `duplicate_datagram` or `reorder_datagram` |
| `applied` | `true`, or `false` when `per_mille` said no for this datagram, which then went as it was |
| `datagram` | `request`, `option_ack`, `ack`, `data`, `error` or `other` (an answer to a FIND) |
| `file` | The file of the datagram's transfer, or `null` |
| `block` | The block number of an `ack` or a `data` datagram, or `null` |
| `to` | Where the datagram was sent, as `127.0.0.1:40001`. The port is the data loader's and is not the same in two runs |
| `size` | The size of the datagram in bytes |

```text
{"event":"started","t":0.003,"program":"adl_mock","version":"0.1.2","events":1,"thw_id_pos":"HNPFMS_L","address":"127.0.0.1","find_port":1001,"tftp_port":59,"faults":["--scenario"],"scenario_features":["wait","refuse","invalid_init_file","drop_status_file","abort","silent","corrupt_served","wrong_checksum","delay","drop_datagram","duplicate_datagram","reorder_datagram"],"scenario":"slow-start.json","scenario_steps":4,"scenario_seed":20261120}
{"event":"scenario_step","t":1.204,"step":1,"trigger":"request","at":1,"operation":"upload","action":"wait","applied":true,"seconds":2}
{"event":"scenario_step","t":3.211,"step":2,"trigger":"request","at":2,"operation":"upload","action":"delay","applied":true,"ms":495,"count":1}
{"event":"scenario_datagram","t":3.211,"step":2,"action":"delay","applied":true,"datagram":"option_ack","file":"HNPFMS_L.LUI","block":null,"to":"127.0.0.1:50321","size":34}
{"event":"stopped","t":9.870,"reason":"operations","operations":1,"scenario_unused":[4],"scenario_idle":[]}
```

The format number of the events stays 1: keys and one event were added, and nothing was removed or changed. A reader that does not know `scenario_step` or `scenario_datagram` never sees them without `--scenario`.

## A file that is refused

`adl_mock` says `adl_mock: --scenario <path>: <sentence>` on the standard error and ends with exit code 5. Every sentence names the step it is about. In a sentence `<n>` is a number, `<key>` is a key and `<action>` an action's word.

| Sentence | What to do |
| --- | --- |
| `the file cannot be read (<reason>)` | The file is not there, is a folder, or cannot be opened. The reason is the system's |
| `the file is larger than 262144 bytes` | A scenario file is not as large as that. Check that it is the file meant |
| `it is not valid JSON, or it has a repeated key, a comment, a trailing comma, text that is not UTF-8, or is nested deeper than 8 levels` | The reader says only that the file is not as strict JSON has it, and not where. Check for a comma after the last member, a comment, a key written twice, and text saved in a code page |
| `the file must be one JSON object` | The file holds a list or a single value |
| `"kind" is missing; it must be "adl-mock-scenario"` | Put the kind at the top of the file |
| `"kind" must be "adl-mock-scenario"` | Another kind of file |
| `this is a scenario for a loader, not for a mock target ("kind" is "adl-loader-scenario")` | The file is for the other program. A scenario for a mock target is the other way round |
| `"format" is missing; it must be 1` | Put the format number at the top |
| `"format" must be 1` | Only the format 1 exists |
| `"format" is 2; this version reads 1, so the file was made for a newer version` | Use a newer version of the program, or the format the file was made for |
| `the file has the key "<key>", which this version does not know` | A mistyped key at the top level |
| `"name" must be text of 1 to 64 characters, with no control character` | The same for `description` (1000 characters) and `comment` (200 characters), with their own limits. A `file` in `on` is text of 1 to 64 characters, and the sentence says `step <n>: "file" must be ...` |
| `"seed" must be a whole number from 0 to 9007199254740991` | Give a whole number in that range |
| `"steps" is missing; it is a list of steps` | Say the steps, a list, which may be empty |
| `"steps" must be a list` | The steps are a list of objects |
| `there are more than 1000 steps` | Make the scenario shorter, or use `every` and `times` |
| `step <n> must be an object` | Each step is an object with `when` and `do` |
| `step <n> has the key "<key>", which this version does not know` | A mistyped key in a step |
| `step <n>: "when" is missing; it says when the step happens` | Give the step a `when` |
| `step <n>: "when" must be an object` | `when` is an object |
| `step <n>: "when" needs exactly one of "request", "status_file" and "after_ms"` | Give one trigger and no more |
| `step <n>: "when" has the key "<key>", which this version does not know` | A mistyped key in `when` |
| `step <n>: "<key>" must be a whole number from <low> to <high>` | A number outside its range, or not whole. The ranges are in the tables above; they are the same for `count`, `copies`, `within_ms` and `block` |
| `step <n>: "<key>" is missing; it is a whole number from <low> to <high>` | A key the action needs |
| `step <n>: "operation" must be one of "information", "upload", "download_media_defined" or "download_operator_defined"` | Use one of the four words |
| `step <n> uses "per_mille" but the scenario has no "seed"` | Give the scenario a `seed`. The same sentence is said for a range of delays, naming `min_ms` |
| `step <n>: "do" is missing; it says what the step does` | Give the step a `do` |
| `step <n>: "do" must be an object` | `do` is an object |
| `step <n>: "action" is missing; it is text of 1 to 32 characters` | Name the action |
| `step <n>: "action" must be a word of lower-case letters, digits and "_"` | The word of an action is one of the twelve in the table above |
| `step <n>: the action "<action>" is not one this version knows (it may be for a newer version)` | The word is mistyped, or the file is for a newer version |
| `step <n>: "<action>" does not go with a "<trigger>" trigger; it goes with <triggers>` | The action needs a trigger that it can act on, as the table says |
| `step <n>: "do" has the key "<key>", which this version does not know` | A key the action does not take |
| `step <n>: "code" must be "0x1000" or "0x1002"` | The two codes a target refuses with |
| `step <n>: "delay" needs "ms", or "min_ms" and "max_ms"` | Give a time, or a range |
| `step <n>: "delay" takes "ms", or "min_ms" and "max_ms", not both` | Give one form |
| `step <n>: "on" and "count" go with the datagram actions and "delay"` | `on` and `count` are for `delay`, `drop_datagram`, `duplicate_datagram` and `reorder_datagram`. Take them out of the step, or use one of those actions |
| `step <n>: "on" must be an object` | `on` is an object with `datagram`, `file` and `block`, or `{}` |
| `step <n>: "on" has the key "<key>", which this version does not know` | A mistyped key in `on` |
| `step <n>: "datagram" must be one of "request", "option_ack", "ack", "data", "error" or "any"` | Use one of the six words |
| `step <n>: "block" goes with "data" or "ack"` | `block` names a block of a transfer, so it goes with the kind `data` or `ack` (or with no kind) |

The usage errors of the command line (`--scenario cannot be used with <option>: the scenario sets the faults`, `--scenario needs a file`) are in [the command line page](command-line.md#messages).

## What a scenario cannot do

- It does not make the target say what a real one would. A scenario is for trying how a loader copes, not for saying that a real target does the same.
- It does not touch what the data loader sends. The faults on a datagram are for what the target sends; the loss of a block or an acknowledgement is seen from the other side as the data loader's retransmission.
- It does not give a second target, a second operation at the same time, or a change to the identity of the target. Those are options of `adl_mock`.
- It does not change a request that has been answered: a fault is for the next thing it acts on.
