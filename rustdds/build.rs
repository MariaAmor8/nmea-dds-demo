use std::{env, path::PathBuf, process::Command};
fn main() {
    let root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap()).join("..");
    let idl = root.join("idl/Navigation.idl");
    let gen = root.join("tools/idl_to_rust.py");
    println!("cargo:rerun-if-changed={}", idl.display());
    println!("cargo:rerun-if-changed={}", gen.display());
    let out = PathBuf::from(env::var("OUT_DIR").unwrap()).join("navigation.rs");
    let status = Command::new("python3")
        .arg(gen)
        .arg(idl)
        .arg(out)
        .status()
        .expect("Necesitas python3 para generar los tipos desde IDL");
    assert!(
        status.success(),
        "No fue posible generar los tipos desde IDL"
    );
}
