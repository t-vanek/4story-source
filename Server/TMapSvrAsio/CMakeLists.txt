# TMapSvrAsio — modernized 4Story map (gameplay) server. Work in
# progress, port-in-progress from the legacy Win32 / MFC / ATL
# Server/TMapSvr/ implementation, modeled on the other modernized
# Asio servers (TLoginSvrAsio, TControlSvrAsio, TPatchSvrAsio,
# TLogSvrAsio): Boost.Asio coroutines, spdlog, toml++, SOCI/ODBC,
# AsioSession from Lib/Own/TNetLib, MessageId from Lib/Own/TProtocol.
#
# Layout
# ------
#   *.h / *.cpp        New modernized sources (added incrementally).
#   legacy_src/        Verbatim copy of Server/TMapSvr/, kept as the
#                      source-of-truth reference material for the
#                      file-by-file port. Not compiled by this target.
#
# No build targets are declared yet. This stub exists so the root
# `add_subdirectory(Server/TMapSvrAsio)` resolves while phase F1
# (main.cpp + minimal executable) is in flight. Targets will be added
# in subsequent commits, mirroring the layout of Server/TPatchSvrAsio/.
