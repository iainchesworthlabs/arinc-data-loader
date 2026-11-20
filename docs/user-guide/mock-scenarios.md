# Scenarios for the mock target

A scenario is a file that says which fault the [mock target](../mock-target.md) shows, and when. It is for a test that has to be run again: the same file gives the same faults at the same moments, so a loader that failed on Tuesday can be shown the same failure on Wednesday. `adl_mock` takes one with the option `--scenario` ([command line](command-line.md#adl_mock)), and the [mock target's window](mock-target.md#a-scenario-file) takes the same file with its **Scenario file...** button.

A scenario is a list of steps. Each step has a trigger (the second request, the fourth status file, twenty seconds into an operation) and an action: any fault the mock target has, or a delay. A seed fixes everything that is left to chance, so that "lose half of the status files" loses the same ones every time.

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
| `per_mille` | A whole number | Optional, 1 to 1000. Each time the trigger occurs the step is applied with this chance in a thousand. Without it, always. Needs a `seed` |
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
| `corrupt_served` | none | Flips a bit in a file the target serves: the initialization file of that request, or the next file it serves | `request`, `status_file`, `after_ms` |
| `wrong_checksum` | none | Writes a wrong check value option: the next one it writes | `status_file`, `after_ms` |
| `delay` | `ms`, 1 to 60000; or `min_ms` and `max_ms`, 0 to 60000, the least not above the most | Holds back the next datagram the target sends by that long. With `min_ms` and `max_ms` the time is drawn from the seed. The datagrams after it follow as usual. TFTP waits for each block, so the whole transfer is that much later | `request`, `status_file`, `after_ms` |

More examples of `do`:

```json
{ "action": "refuse", "code": "0x1002" }
{ "action": "delay", "ms": 300 }
{ "action": "wait", "seconds": 5 }
{ "action": "silent" }
```

An action waits for the thing it acts on: the next status file, the next file served, the next check value written, the next datagram. One that finds nothing to act on when the operation ends is dropped. A new action word in a later format is refused by this version, because running half a scenario would mislead.

## The seed

Chance appears in two places only: `per_mille`, and a range of delays. Without either, the seed is never used. A scenario that uses one and has no `seed` is refused.

The numbers come from SplitMix64, written out in integer arithmetic only, so that every system and every standard library gives the same ones. The `seed` starts a generator, and its nth output starts the generator of step n. A step takes one number for `per_mille` (if it has one) and then one for a range of delays (if it has one) at each time its trigger occurs, whether or not the chance says yes, so that what a step draws depends on how many times it has occurred and on nothing else. A number in a range is drawn by rejection, so that no number is likelier than another. A chance of `per_mille` applies when a draw from 1 to 1000 is at most that number.

The numbers are the same on every run, so the faults of a run are the same, if the loader does the same. Time is the exception: a step with `after_ms` is as steady as the clock and the loader are, and the order of two things that happen a few milliseconds apart can change from one run to the next.

## Running `adl_mock` with a scenario

`--scenario <file>` takes the faults from the file in place of the fault options. It cannot be given with `--wait-first`, `--wait-every`, `--refuse`, `--refuse-code`, `--invalid-init-file`, `--drop-status-every`, `--corrupt-served`, `--wrong-checksum`, `--abort-after` or `--silent`; the program says which. The identity, the files, the network, the pace, `--operations`, `--seconds` and `--json` can be given with it. A scenario with a step that goes silent needs `--seconds`, as `--silent` does, because the operation never ends.

The file is read before anything is opened. If it cannot be used the program writes one sentence and ends with exit code 5; nothing is written on the standard output.

With `--scenario` the events change in three ways. Everything else is as in [the command line](command-line.md#what-it-writes).

- `started` has `"--scenario"` in `faults`, and the keys `scenario` (the path as it was given), `scenario_steps` (the number of steps) and `scenario_seed` (the seed, or `null`).
- Every time the trigger of a step occurs, a `scenario_step` event is written, before what the action causes.
- `stopped` has `scenario_unused`: the numbers of the steps that never fired. A step that never fired is not an error; this is how a run that did not do what the file says can be seen.

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

```text
{"event":"started","t":0.003,"program":"adl_mock","version":"0.1.2","events":1,"thw_id_pos":"HNPFMS_L","address":"127.0.0.1","find_port":1001,"tftp_port":59,"faults":["--scenario"],"scenario":"slow-start.json","scenario_steps":4,"scenario_seed":20261120}
{"event":"scenario_step","t":1.204,"step":1,"trigger":"request","at":1,"operation":"upload","action":"wait","applied":true,"seconds":2}
{"event":"scenario_step","t":3.211,"step":2,"trigger":"request","at":2,"operation":"upload","action":"delay","applied":true,"ms":495}
{"event":"stopped","t":9.870,"reason":"operations","operations":1,"scenario_unused":[4]}
```

The format number of the events stays 1: keys and one event were added, and nothing was removed or changed. A reader that does not know `scenario_step` never sees it without `--scenario`.

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
| `"name" must be text of 1 to 64 characters, with no control character` | The same for `description` (1000 characters) and `comment` (200 characters), with their own limits |
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
| `step <n>: "<key>" must be a whole number from <low> to <high>` | A number outside its range, or not whole. The ranges are in the tables above |
| `step <n>: "<key>" is missing; it is a whole number from <low> to <high>` | A key the action needs |
| `step <n>: "operation" must be one of "information", "upload", "download_media_defined" or "download_operator_defined"` | Use one of the four words |
| `step <n> uses "per_mille" but the scenario has no "seed"` | Give the scenario a `seed`. The same sentence is said for a range of delays, naming `min_ms` |
| `step <n>: "do" is missing; it says what the step does` | Give the step a `do` |
| `step <n>: "do" must be an object` | `do` is an object |
| `step <n>: "action" is missing; it is text of 1 to 32 characters` | Name the action |
| `step <n>: "action" must be a word of lower-case letters, digits and "_"` | The word of an action is one of the nine in the table above |
| `step <n>: the action "<action>" is not one this version knows (it may be for a newer version)` | The word is mistyped, or the file is for a newer version |
| `step <n>: "<action>" does not go with a "<trigger>" trigger; it goes with <triggers>` | The action needs a trigger that it can act on, as the table says |
| `step <n>: "do" has the key "<key>", which this version does not know` | A key the action does not take |
| `step <n>: "code" must be "0x1000" or "0x1002"` | The two codes a target refuses with |
| `step <n>: "delay" needs "ms", or "min_ms" and "max_ms"` | Give a time, or a range |
| `step <n>: "delay" takes "ms", or "min_ms" and "max_ms", not both` | Give one form |

The usage errors of the command line (`--scenario cannot be used with <option>: the scenario sets the faults`, `--scenario needs a file`) are in [the command line page](command-line.md#messages).

## What a scenario cannot do

- It does not make the target say what a real one would. A scenario is for trying how a loader copes, not for saying that a real target does the same.
- It does not give a second target, a second operation at the same time, or a change to the identity of the target. Those are options of `adl_mock`.
- It does not change a request that has been answered: a fault is for the next thing it acts on.
