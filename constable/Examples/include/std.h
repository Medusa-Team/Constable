
/ "fs" clone by getfile getfile.filename;
/ "proc" by exec primaryspace(file,@"fs");
// / "domain" test_enter of process;
/ "domain" of process;

function constable_init;
function _init
{
	transparent process constable;
	constable.pid=constable_pid();
	fetch constable;
	constable_init();
	update constable;
}
