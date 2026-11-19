---
template: marketing.html
title: Support
title_tag: "Support: ARINC Data Loader"
description: How to get help with ARINC Data Loader, how to report a problem or a security issue, how to ask about the paid editions, and what support each edition includes.
layout: prose
---
# Support

## Before you ask

- The [guide](user-guide/index.md) is organised by task. [Troubleshooting](user-guide/troubleshooting.md) lists what the programs show and what to check, starting with the most common.
- [FAQ](faq.md) and the guide's [longer list of questions](user-guide/faq.md) answer the questions that come up first.
- [What has been checked](status.md) says what has not been tried, so you know whether a problem is a known gap.

## Report a problem

Open an issue on the project's [GitHub issues](https://github.com/iainchesworthlabs/arinc-data-loader/issues) page and use the bug report form. It asks for:

- the version, which About shows, and the platform;
- what happened and what you expected;
- the steps to reproduce it;
- the activity log text and the text of any dialog.

The smallest load or header file that shows the problem helps most. There is a form for feature requests too. For more than a small change, open an issue before you send a pull request.

**Do not attach a load, a media set or a capture taken from a real aircraft or from a customer's equipment.** Make a small one that shows the problem.

## Report a security problem

Report it privately, not as a public issue: on the repository's Security tab choose "Report a vulnerability". If you cannot use that, open an issue that says only that you have a security matter, with no details, and the maintainer will arrange a private channel. [The security policy](https://github.com/iainchesworthlabs/arinc-data-loader/blob/main/SECURITY.md) says what counts. The programs do not authenticate a target or encrypt anything, because ARINC 615A over TFTP has neither, and a report that depends on that is a limit of the protocol.

## Talk to us

For Pro and Enterprise, a quote, a trial, the qualification kit or a port to your platform, tell us what you load, onto what, how many seats and which versions you use.

The form goes to one person and to no one else. Nothing you write is published, and there is no automatic reply: we answer by email, from our own address. Please put nothing confidential in it, and do not paste a load, a media set or a capture from a real aircraft or a customer's equipment. <a href="/privacy/#contact-form">What the form keeps, and for how long</a>.

<form id="contact-form" class="adl-form" method="post" action="/api/contact">
<input type="hidden" name="source" value="adl">
<p class="adl-form__help">Every field is needed except the organisation.</p>
<div>
<label for="cf-name">Your name</label>
<input id="cf-name" name="name" type="text" autocomplete="name" maxlength="100" required>
</div>
<div>
<label for="cf-organisation">Organisation <span class="adl-form__opt">(optional)</span></label>
<input id="cf-organisation" name="organisation" type="text" autocomplete="organization" maxlength="150">
</div>
<div>
<label for="cf-email">Your email address</label>
<input id="cf-email" name="email" type="email" autocomplete="email" maxlength="254" required aria-describedby="cf-email-help">
<p id="cf-email-help" class="adl-form__help">This is where we reply. Give one address.</p>
</div>
<div>
<label for="cf-topic">What are you asking about?</label>
<select id="cf-topic" name="topic" required>
<option value="">Choose one</option>
<option value="pro">The Pro edition</option>
<option value="enterprise">The Enterprise edition</option>
<option value="qualification-kit">The qualification kit</option>
<option value="platform-work">A port to your platform</option>
<option value="other">Something else</option>
</select>
</div>
<div>
<label for="cf-message">Your message</label>
<textarea id="cf-message" name="message" rows="7" minlength="10" maxlength="4000" required aria-describedby="cf-message-help"></textarea>
<p id="cf-message-help" class="adl-form__help">At least 10 characters and at most 4,000, with no more than two web addresses.</p>
</div>
<div class="adl-form__trap" aria-hidden="true">
<label for="cf-ref">Leave this field empty</label>
<input id="cf-ref" name="ref_code" type="text" tabindex="-1" autocomplete="off">
</div>
<div class="adl-form__consent">
<input id="cf-consent" name="consent" type="checkbox" value="yes" required>
<label for="cf-consent">I agree that Iain Chesworth Labs may use what I send here to answer me. <a href="/privacy/#contact-form">What is kept, and for how long</a>.</label>
</div>
<button class="icl-button" type="submit">Send message</button>
</form>

## What support each edition includes

| Edition | Support |
| --- | --- |
| Community | Help from the community on the GitHub issues page. The project is maintained by one person, so there is no guaranteed response time and no promised schedule for a fix. Until version 1.0, only the latest release receives fixes. |
| Pro | A private support channel with response targets, help with use and integration, supported versions with security fixes, and an update policy. |
| Enterprise | A support agreement for the site, the [qualification kit](qualification-kit.md) for each supported version, and a version kept supported for as long as a kit relies on it. |

<!-- OWNER DECISION: the support channel, hours, response times, supported versions and update policy for the paid editions. -->

[Pricing](pricing.md) compares the editions.
