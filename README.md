## simpsh

a simple (mostly) shell.

This is still a WIP, there are things that aren't implemented yet, and there are definitely still bugs.

The main shell is now implemented. I'm currently working on adding in many of the text processing/math unix tools
as builtins.

The main goal of this shell is going to be to highly optimize for performance over everything.
Where bash tries being full featured (to the point of breaking posix conformance), along with zsh, some shells
try above everything else to be posix compliant, and others like dash do try to keep things lean, and fast.
The eventual goal of mine, will be to make it run scripts/commands as fast as possible. Which means, in some instances using
more memory than otherwise needed, or sacrificing simplicity in some areas, in order to gain performance.

As of right now, it's using editline for line editing. Mostly because readline proved to be a pain once things
like signal handling and job control got involved. Editline mostly solves those problems, but with the sacrifice of
not getting all of the niceties readline provides. It also can be built without editline if interactive use isn't needed.

There are a few features beyond the posix spec, in the name of performance that are planned for this shell, but until
everything needed for posix is done they will just remain on the roadmap.
