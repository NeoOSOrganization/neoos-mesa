# neoos-mesa

Sub-project 1 of the Gallium3D-on-NeoOS goal (see NeoOS's own
`docs/project-goal.md`): Mesa's OSMesa target (real OpenGL API +
`softpipe` software pipe driver), cross-compiled for NeoOS with the
hosted `x86_64-neoos-linux-musl` toolchain.

No windowing-system integration yet -- `test/triangle_test.c` renders
off-screen via OSMesa's own API. See
`docs/superpowers/specs/2026-09-22-gallium3d-osmesa-bringup-design.md`
in the neoos-kernel repo for the full design.
