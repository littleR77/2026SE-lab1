"""按实验二提交要求打包：实验2_学号_姓名.zip

实验2_学号_姓名/
├── 实验报告.pdf
├── UML/      各图的 .png 与 .puml
└── UI界面/   界面1.png … 界面n.png
"""
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
NAME = "实验2_241220115_任虹霖"

files = [(ROOT / "report" / "实验报告.pdf", "实验报告.pdf")]
files += [(p, f"UML/{p.name}") for p in sorted((ROOT / "UML").iterdir()) if p.suffix in (".png", ".puml")]
ui = sorted((ROOT / "UI界面").glob("界面*.png"), key=lambda p: int(p.stem[2:]))
files += [(p, f"UI界面/{p.name}") for p in ui]

out = ROOT / f"{NAME}.zip"
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for src, arc in files:
        z.write(src, f"{NAME}/{arc}")

print(out.name, len(files), "files")
for _, arc in files:
    print(" ", arc)
