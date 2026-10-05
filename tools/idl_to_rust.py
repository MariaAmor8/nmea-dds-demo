"""Generador deliberadamente limitado: un modulo, un struct @topic sin clave.
Rechaza cualquier sintaxis fuera del subconjunto; no es un compilador OMG IDL general.
"""
import re
import sys
from pathlib import Path

def generate(text):
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    match = re.fullmatch(r"\s*module\s+(\w+)\s*\{\s*@topic\s+struct\s+(\w+)\s*\{(.*?)\}\s*;\s*\}\s*;\s*", text, re.S)
    if not match:
        raise ValueError("IDL fuera del subconjunto soportado: module + @topic struct")
    module, name, body = match.groups()
    mapping = {"unsigned long": "u32", "unsigned long long": "u64",
               "long": "i32", "double": "f64", "boolean": "bool"}
    fields = []
    names = set()
    for field in body.split(";"):
        if not field.strip():
            continue
        m = re.fullmatch(r"\s*(unsigned\s+long\s+long|unsigned\s+long|long|double|boolean)\s+([A-Za-z_][A-Za-z_0-9]*)\s*", field)
        if not m:
            raise ValueError("Campo IDL no soportado: " + field.strip())
        kind, field_name = m.groups()
        kind = " ".join(kind.split())
        if field_name in names:
            raise ValueError("Campo duplicado: " + field_name)
        names.add(field_name)
        fields.append(f"    pub {field_name}: {mapping[kind]},")
    if not fields:
        raise ValueError("Struct sin campos")
    return '\n'.join([
        '// Generado desde idl/Navigation.idl. No editar.',
        f'pub const TYPE_NAME: &str = "{module}::{name}";',
        '#[derive(serde::Serialize, serde::Deserialize, Debug, Clone)]',
        f'pub struct {name} {{', *fields, '}', ''])

if __name__ == "__main__":
    try:
        Path(sys.argv[2]).write_text(generate(Path(sys.argv[1]).read_text()), encoding="utf-8")
    except Exception as e:
        sys.exit(str(e))
