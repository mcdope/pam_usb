# Introduction
First off, thank you for considering contributing to pam_usb. 
Without the community formed by people like you this project would have been dead since many years.

Following these guidelines helps to communicate that you respect the time of the developers managing 
and developing this open source project. In return, they should reciprocate that respect in addressing 
your issue, assessing changes, and helping you finalize your pull requests.

pam_usb is an open source project and we love to receive contributions from our community — you! 
There are many ways to contribute, from writing tutorials or blog posts, improving the documentation, 
submitting bug reports and feature requests or writing code which can be incorporated into pam_usb itself.

# Rules for contributing

## Responsibilities
- Ensure that code works in each scenario, not only local but also remote and that it doesn't break remote check
- Ensure that code is fully tested manually, and by the scripts provided in `tests`
- Tackle the issue at the root, be sure you've found the correct place to fix it
- If you change config related things, make sure it works for upgrade installs
- Create issues for any major changes and enhancements that you wish to make. Discuss things transparently and get community feedback
- Adopt the existing code-style (yes, it's bad in many places but for now we keep it)
- Keep feature versions as small as possible, preferably one new feature per version
- Be welcoming to newcomers and encourage diverse new contributors from all backgrounds. See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Any contributor in the future will be expected to follow it, so contributing to pam_usb will be taken as a "silent approval" of the CoC

## Your first contribution
Unsure where to begin contributing to pam_usb? You can start by looking through these beginner and help-wanted issues: 
- "good-first-issue" issues - issues which should only require a few lines of code, and a test or two. 
- "help-wanted" issues - issues which should be a bit more involved than beginner issues. 

In general you can always find stuff to work on in the open issues.


## How to report bugs

If you find a security vulnerability, do NOT open an issue. Email tobiasbaeumer@gmail.com instead. PGP Encryption is available, just use the same key I use for package signing. You can find it on the keyservers or my APT repo.

In order to determine whether you are dealing with a security issue, ask yourself these two questions:

- Can I access something that's not mine, or something I shouldn't have access to?
- Can I disable something for other people?
- Can I circumvent the local check?

If the answer to either of those questions are "yes", then you're probably dealing with a security issue. Note that even if you answer "no" to all questions, you may still be dealing with a security issue, so if you're unsure, just email me at tobiasbaeumer@gmail.com.

## When filing an issue, make sure to provide these things:

- What version of pam_usb are you using?
- What operating system at which release version you're using?
- What did you do?
- What did you expect to see?
- What did you see instead?
- Provide the output of `w` and `pamusb-check --debug $USER`
- If the behaviour happens in a graphical environment provide its name (gnome, kde, xfce etc.) and login manager used (xdm, gdm etc.)

# How pull request reviews are handled

Pull requests are reviewed at least once a week, usually soon after receiving the notification about it. After feedback has been given we expect responses within two weeks. After two weeks we may close the pull request if it isn't showing any activity.

# Community

You can reach core devs via Github discussions, or via Hangouts and Email at <tobiasbaeumer@gmail.com>.


