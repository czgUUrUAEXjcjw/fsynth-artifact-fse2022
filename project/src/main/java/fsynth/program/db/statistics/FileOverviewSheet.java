package fsynth.program.db.statistics;

import fsynth.program.subject.Oracle;
import fsynth.program.subject.Subject;
import fsynth.program.InputFormat;
import fsynth.program.db.FileDatabase;
import fsynth.program.db.FileRecord;
import fsynth.program.subject.Subjects;

import javax.annotation.Nonnull;
import java.nio.file.Path;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.stream.Stream;

/**
 * @author anonymous
 * @since 2020-05-14
 **/
@BuildStatistics
@ForEachFormatStatistics
public final class FileOverviewSheet extends CSVReport {
    @Nonnull
    private final InputFormat format;

    /**
     * Instantiate a new FileOverviewSheet
     *
     * @param format Format, case-insensitive. MUST be the same as annotated in the annotations of the subject
     */
    public FileOverviewSheet(InputFormat format) {
        super(format.toString().toLowerCase() + "_file_overview_sheet", "File Overview Sheet for " + format);
        this.format = format;
    }

    @Override
    final void buildTable() {
        final String sizeHeader = "Size";
        List<Subject> subjects = Oracle.getSubjectsFor(this.format);
        this.addColumnHeader("File name");
        this.addColumnHeader(sizeHeader);
        this.addColumnHeader(Subjects.INVALID.toString(), "ANTLR");
        subjects.forEach(subject -> this.addColumnHeader(subject.getKind().toString()));
        this.addGeneralHeader("File names, FailReason for each subject and file sizes in Bytes for each file. Empty=Success");
        Stream<Map.Entry<Path, FileRecord>> files;
        final FileDatabase fileDatabase = super.getDatabase();
        if (fileDatabase == null) {
            throw new NullPointerException("This should not happen");
        }
        files = fileDatabase.getForFormat(this.format);
        files.sorted(Comparator.comparing(entry -> entry.getValue().toString())).forEach(file -> {
            Stream.concat(Stream.of(Subjects.INVALID), subjects.stream().map(Subject::getKind)).forEach(subject -> {
                this.set(file.getKey().toString(), subject.toString(), file.getValue().getFailReason(subject));
            });
            this.set(file.getKey().toString(), sizeHeader, Long.toString(file.getValue().getSize(), 10));
        });
    }
}
