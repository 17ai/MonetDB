SET optimizer = 'sequential_pipe';
declare opt_pipe_def string; 
set opt_pipe_def  = (select def from sys.optimizers() where name = optimizer);
set optimizer = substring(opt_pipe_def, 0, length(opt_pipe_def)-length('optimizer.joinPath();optimizer.reorder();optimizer.deadcode();optimizer.reduce();optimizer.matpack();optimizer.querylog();optimizer.multiplex();optimizer.garbageCollector();')) || 'optimizer.reorder();optimizer.deadcode();optimizer.reduce();optimizer.matpack();optimizer.querylog();optimizer.multiplex();optimizer.DVframeworkP();optimizer.garbageCollector();';
