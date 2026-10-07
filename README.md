# fastchat

**FastChat is a deliberately small ChatGPT controller for the cheapest phone we can get.**

The use case is simple: put a fast, low-overhead interface on a very cheap Android phone and use it to talk to agents. The phone does not need to be a general-purpose AI workstation. It needs to let a human read a thread, type or dictate, send work to an agent, and get the result back without the interface overwhelming the hardware.

## Design status: deliberately open

The feature set and Android process model are **not frozen**.

A couple of ChatGPT clones are not enough evidence for either one, and source review cannot replace physical observation. In particular, the Shizuku/runtime inventory of the current ChatGPT app on the target phones is not complete yet, so FastChat must not assume that the processes, services, WebView/renderer behavior, media paths, background work, or lifecycle boundaries already identified are the whole set.

Implementation preferences — including small native code, C built through ICK where appropriate, D experiments, and Idriç type sketches — do not substitute for requirements discovery.

See [reference-code/](reference-code/) for the curated reference map and [the dated broad GitHub sweep](reference-code/sweep-2026-10-06.md). The sweep is intentionally wider than the code FastChat is likely to use.

The independent [Compose Material 3 comparison baseline](android-material3/README.md)
uses deterministic transport and disk-first durable response replay. It remains
a draft comparison lane pending physical MIRO A1 qualification.

The reference target is the **MIRO A1**, an Android 14 Go phone in roughly the $60 class.

[Buy the MIRO A1 on Amazon — affiliate link](https://www.amazon.com/dp/B0FJX2FTZF?tag=macguyver03-20)

Amazon prices move. The directly recoverable Amazon listing for ASIN `B0FJX2FTZF` has recently been above $60, while current listings for the same phone remain around the $60 mark. The project target is therefore the **~$60 phone**, not a frozen claim that a retailer will always hold one exact price.

## Why this exists

Dedicated agent computers are nifty. The [Lapis One](https://www.pamir.ai/) is currently advertised around **$499**, with a **$599** regular/compare-at price depending on batch and timing. If Pamir wants to send me one, I will definitely use it.

But I can talk to my agents with just a phone.

That changes the engineering question. Instead of asking how much computer an AI interface can consume, FastChat asks how little computer the human side actually needs. The expensive compute can live elsewhere. The controller should remain small, responsive, inspectable, and cheap.

The intended hardware baseline is deliberately severe: the cheapest practical phone available on Amazon, rather than a flagship phone or a purpose-built $500–$600 agent appliance.

## Cheap hardware is also a compiler target

This phone is not only a UI target. It is physical acceptance hardware for compiler work:

- [ICK](https://github.com/dilapidated-shed/ick) — compiler/toolchain work including Android ARM targets.
- [Idriç ARM/Thumb](https://github.com/fuego-ironworks/idric-arm-thumb) — work in progress on ARM/Thumb compiler back ends and physical-phone execution.

Those projects are works in progress developing compiler back ends specifically for the cheapest phone available on Amazon. The point is not to spend the extra hardware budget on wider assumptions. It is to make useful software run on the cheap machine we actually have.

FastChat should eventually be able to consume ChatGPT threads as one possible front end to the same broader thread/skein model. It should not require a large browser-shaped application merely to talk to an agent.

[MIRO A1 on Amazon — affiliate link](https://www.amazon.com/dp/B0FJX2FTZF?tag=macguyver03-20)

**Full luxury communism now.**
