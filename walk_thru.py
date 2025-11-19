import os

def dump_source_files(root_dirs, output_file="output.md"):
    if isinstance(root_dirs, str):
        root_dirs = [root_dirs]

    with open(output_file, "w", encoding="utf-8") as out:
        for root_dir in root_dirs:
            for dirpath, _, filenames in os.walk(root_dir):
                for name in filenames:
                    if name.endswith((".cpp", ".h")):
                        full_path = os.path.join(dirpath, name)

                        out.write(f"# {full_path}\n\n")
                        out.write("```cpp\n")

                        try:
                            with open(full_path, "r", encoding="utf-8") as f:
                                out.write(f.read())
                        except Exception as e:
                            out.write(f"<!-- Error reading file: {e} -->\n")

                        out.write("\n```\n\n")


if __name__ == "__main__":
    # Add as many root directories as you want
    roots = ["src", "include"]
    dump_source_files(roots)
