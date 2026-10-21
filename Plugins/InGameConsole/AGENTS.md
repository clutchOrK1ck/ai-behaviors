# Project description

An in-game console with autocompletions and commands defined as blueprints.

# Architecture and comments

- commands are expected to be defined as blueprints. If c++ functionality is needed, it must expose itself to blueprints
- commands can be normal or latent. Latent commands can execute over multiple frames
- clients can execute non-latent commands on the server
- a command executor object is responsible for executing commands. The game console owns one. When executing remotely, a separate executor associated with the calling controller is used