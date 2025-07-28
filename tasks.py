import invoke
import shutil
from pathlib import Path
from typing import Optional


@invoke.task
def build(ctx, toolchain: Optional[str] = None, build_type: str = "Debug", build_dir: str = "build/",
          clean: bool = False):
    cmake_modes = {"Release", "Debug", "RelWithDebInfo", "MinSizeRel"}
    if build_type not in cmake_modes:
        raise ValueError(f"Invalid mode. --mode must be one of {sorted(cmake_modes)}")

    if toolchain and not Path(toolchain).exists():
        raise ValueError(f"Invalid toolchain file: {toolchain}")

    config_cmd = ["cmake", "-S", ".", "-B", build_dir, "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={build_type}"]
    if toolchain:
        config_cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")

    if clean:
        shutil.rmtree(build_dir, ignore_errors=True)
        shutil.rmtree("bin", ignore_errors=True)

    ctx.run(" ".join(config_cmd), echo=True)

    with ctx.cd(build_dir):
        ctx.run("cmake --build . --target install", echo=True)
