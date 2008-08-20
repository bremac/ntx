XXX: Streamline flow into development and explanation sections.
XXX: Expand information present in all sections, especially about usuage.

NTX is a command-line note organizer, based around the concept of tagged
notes. Each note is identified by one or more tags, and seaches may be
performed on the intersection of several tags at once. This approach
is very flexible, and allows for more faster and more intuitive organization.
For example, one could store memory leak bugs for ntx with the tags
'ntx', 'bug', 'leak', 'todo', effectively placing the note in several
relevant categories at once.

NTX is run from the command line as 'ntx', followed by the name of the
action to perform - for a brief summary, see 'ntx --help'. The interface of
NTX is modelled after git, and thus notes are presented in lists with their
first line as a summary, as well as a four-digit identifier which must be
supplied to any operation targetting a specific note. Any editing operations
are performed under POSIX systems uses the value of the EDITOR environment
variable as the command to be run, with the name of the file as the parameter.

NTX was written solely with two goals in mind: elegance, and speed. As such,
the internal records maintained by NTX in the directory pointed to by the
NTXDIR environment variable (or ~/.ntx, if NTXDIR is unset) are maintained in
gzipped text files. Each set of records is grouped into a file either by
purpose or by a prefix, and each individual record consists of the
hexidecimal identifier of the record, a tabstop character, and the relevant
information for the record. For more information on the internal record-
keeping employed in NTX, consult the source code of ntx.c, or the files
themselves via zcat. Though undocumented, the format should be simple enough
to glean a complete understanding of their purpose from their contents.

Due to the many points of failure in the NTX source, it is also equipped with
a simple exception-handling mechanism, derived from the cexcept project
(see http://cexcept.sourceforge.net for more information.) This mechanism is
expanded with a simple allocation interface to ensure that memory is correctly
freed after an exception is thrown, and that default destructors may be called
correctly to do so. Though this interface is rudimentary and primitive, it is
key to keeping the internals of NTX simple, and largely free from complex
error-handling structures. It is advised that this interface be used for any
extension of NTX, unless it is undertaken to port this project to a more
exception-friendly language.

Portability was also another major consideration in constructing NTX. The
approach favoured by the Plan 9 system, of relegating system-specific
components of specific source files, was used, with the number of such calls
kept to a minimum. For those interested in porting NTX to an unsupported
system, please refer to the document 'PORTING.txt'.

A simple series of integration tests for NTX can also be found under the
'tests/' directory, in the form of bash scripts. Additional tests may be added
to the set of integration tests by creating a new script in this folder, with
a filename containing the prefix 'test-'. Tests may be run either directly, by
changing to the test directory and running './test.sh', or by issuing the
command 'make test' from the top directory of the source tree.

For up-to-date versions and news about NTX, please visit
http://macdonellba.googlepages.com/ntx.html
