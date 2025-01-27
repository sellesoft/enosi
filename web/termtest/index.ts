var term = await import("terminal-kit");

async function test() {
    var spinner = await term.spinner( 'unboxing-color' ) ;
    term( ' Loading... ' ) ;
}


test() ;


