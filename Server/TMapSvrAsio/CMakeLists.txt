# TMapSvrAsio — modernization in progress (PR #23 onward).
#
# This directory currently holds the legacy Server/TMapSvr/ sources
# copied verbatim in commit b6d4a87, with incremental cleanup commits
# on top (Log → spdlog, BR/Bow settings → constexpr, Win32 / COM / VS
# artefacts dropped, CP949 → UTF-8). The legacy sources still depend on
# Win32 / MFC / ATL and are *not* built — they're parked in the tree as
# the source material for the file-by-file port to the new Asio /
# Boost.Asio / spdlog / SOCI stack used by the other modernized servers
# (TLoginSvrAsio, TControlSvrAsio, TPatchSvrAsio, TLogSvrAsio).
#
# No build targets are declared yet. This empty stub exists so the
# root `add_subdirectory(Server/TMapSvrAsio)` at CMakeLists.txt:185
# resolves without error.  Targets (tmapsvr_asio_core static library,
# tmapsvr_asio executable, tests) will be added back as the new
# scaffolding lands, mirroring the layout of Server/TPatchSvrAsio/.
