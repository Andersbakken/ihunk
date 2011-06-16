#include <QtCore>

struct Line {   
    enum Type {
        Other, // git stuff etc
        MinusMinusMinus,
        PlusPlusPlus,
        AtAt,
        Context,
        Removed,
        Added
    } type;
    QString line;
};

static inline void processHunk(const QList<Line> &lines,
                               const QRegExp &removedRx,
                               const QRegExp &addedRx, bool all)
{
    const int count = lines.size();
    for (int i=0; i<count; ++i) {
        const QRegExp *rx = 0;
        switch (lines.at(i).type) {
        case Line::Removed:
            rx = &removedRx;
            break;
        case Line::Added:
            rx = &addedRx;
            break;
        default:
            continue;
        }
        if (!rx->isEmpty()) {
            if (lines.at(i).line.mid(1).contains(*rx)) {
                if (!all)
                    return; // hunk ignored
            } else if (all) {
                // print hunk
                break;
            }
        }
    }
    for (int i=0; i<count; ++i) {
        printf("%s\n", qPrintable(lines.at(i).line));
    }
}

static inline bool isHunkContent(const QString &line)
{
    switch (line.at(0).toLatin1()) {
    case '-':
    case '+':
    case ' ':
        return true;
    default:
        break;
    }
    return false;
}

int main(int argc, char **argv)
{
    QString fileName;
    enum Mode {
        One,
        All
    } mode = One;
    QRegExp addedRx, removedRx;
    for (int i=1; i<argc; ++i) {
        const QByteArray arg = QByteArray::fromRawData(argv[i], strlen(argv[i]));
        if (arg == "--all" || arg == "-a") {
            mode = All;
        } else if (arg == "--one" || arg == "-1") {
            mode = One;
        } else if (arg.size() > 8 && arg.startsWith("--added=")) {
            addedRx.setPattern(arg.mid(8));
        } else if (arg.size() > 3 && arg.startsWith("-+=")) {
            addedRx.setPattern(arg.mid(3));
        } else if (arg.size() > 10 && arg.startsWith("--removed=")) {
            removedRx.setPattern(arg.mid(10));
        } else if (arg.size() > 3 && arg.startsWith("--=")) {
            removedRx.setPattern(arg.mid(3));
        } else {
            fileName = QString::fromLocal8Bit(arg);
        }
    }
    if (addedRx.isEmpty() && removedRx.isEmpty()) {
        qWarning("No rx specified");
        return 1;
    }
    QFile f;
    if (fileName.isEmpty()) {
        if (!f.open(stdin, QIODevice::ReadOnly)) {
            qWarning("Can't open stdin for reading");
            return 1;
        }
    } else {
        f.setFileName(fileName);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning("Can't open %s for reading", qPrintable(fileName));
            return 1;
        }
    }

    QTextStream ts(&f);
    enum State {
        LookingForMinusMinusMinus,
        LookingForPlusPlusPlus,
        LookingForAtAt,
        GatheringHunk
    } state = LookingForMinusMinusMinus;
    QList<Line> lines;
    int lineNumber = 0;
    while (!ts.atEnd()) {
        ++lineNumber;
        const QString line = ts.readLine();
        if (line.isEmpty()) {
            qWarning("Unexpected empty line %d", lineNumber);
            continue;
        }

        if (state == GatheringHunk && !isHunkContent(line)) {
            processHunk(lines, removedRx, addedRx, mode == All);
            lines.clear();
            state = LookingForMinusMinusMinus;
        }
        switch (state) {
        case LookingForMinusMinusMinus:
            if (line.startsWith(QLatin1String("---"))) {
                const Line l = { Line::MinusMinusMinus, line };
                lines.append(l);
                state = LookingForPlusPlusPlus;
            } else {
                const Line l = { Line::Other, line };
                lines.append(l);
            }
            break;
        case LookingForPlusPlusPlus:
            if (line.startsWith(QLatin1String("+++"))) {
                const Line l = { Line::PlusPlusPlus, line };
                lines.append(l);
                state = LookingForAtAt;
            } else {
                qWarning("Unexpected line here %s. I wanted a line that starts with +++", qPrintable(line));
                const Line l = { Line::Other, line };
                lines.append(l);
            }
            break;
        case LookingForAtAt:
            if (line.startsWith(QLatin1String("@@"))) {
                const Line l = { Line::AtAt, line };
                lines.append(l);
                state = GatheringHunk;
            } else {
                qWarning("Unexpected line here %s. I wanted a line that starts with +++", qPrintable(line));
                const Line l = { Line::Other, line };
                lines.append(l);
            }
            break;
        case GatheringHunk:
            Line::Type type = Line::Other;
            switch (line.at(0).toLatin1()) {
            case '+':
                type = Line::Added;
                break;
            case '-':
                type = Line::Removed;
                break;
            case ' ':
                type = Line::Context;
                break;
            default:
                qWarning("Unexpected content here in GatheringHunk [%s]", qPrintable(line));
                break;
            }
            if (type != Line::Other) {
                const Line l = { type, line };
                lines.append(l);
            }
            break;
        }
    }
    if (state == GatheringHunk)
        processHunk(lines, removedRx, addedRx, mode == All);
    return 0;
}
