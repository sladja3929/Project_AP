//PostToolUse(Edit|Write) 훅: Source 하위 .h/.cpp의 CLAUDE.md 코드 컨벤션 검사
//-주석 공백("// 텍스트" -> "//텍스트")은 편집 영역에 한해 자동 수정
//-Allman 중괄호, 제어문 전후 빈 줄은 편집 영역에 한해 위반을 보고(exit 2 -> Claude에 피드백)
const fs = require('fs');

function readStdin()
{
    try
    {
        return JSON.parse(fs.readFileSync(0, 'utf8'));
    }
    catch (e)
    {
        return null;
    }
}

const input = readStdin();

if (!input || !input.tool_input)
{
    process.exit(0);
}

const filePath = (input.tool_input.file_path || '').replace(/\\/g, '/');

if (!/\/Source\/.*\.(h|cpp)$/i.test(filePath) || !fs.existsSync(filePath))
{
    process.exit(0);
}

let content = fs.readFileSync(filePath, 'utf8');
const region = input.tool_name === 'Write' ? content : (input.tool_input.new_string || '');

if (!region.trim())
{
    process.exit(0);
}

//1. 주석 공백 자동 수정 (편집 영역만)
const fixedRegion = region
    .split('\n')
    .map((line) =>
    {
        if (/^\s*\/\/ Copyright/.test(line))
        {
            return line;
        }

        return line
            .replace(/^(\s*)\/\/ +(?=\S)/, '$1//')
            .replace(/([;{})]\s+)\/\/ +(?=\S)/, '$1//');
    })
    .join('\n');

let fixedCount = 0;

if (fixedRegion !== region && content.includes(region))
{
    const before = region.split('\n');
    const after = fixedRegion.split('\n');
    fixedCount = before.filter((l, i) => l !== after[i]).length;
    content = input.tool_name === 'Write' ? fixedRegion : content.split(region).join(fixedRegion);
    fs.writeFileSync(filePath, content, 'utf8');
}

//2. 편집 영역의 라인 범위 계산
const lines = content.split('\n');
const ranges = [];
let searchFrom = 0;

while (true)
{
    const idx = content.indexOf(fixedRegion, searchFrom);

    if (idx < 0)
    {
        break;
    }

    const startLine = content.slice(0, idx).split('\n').length - 1;
    ranges.push([startLine, startLine + fixedRegion.split('\n').length - 1]);
    searchFrom = idx + fixedRegion.length;
}

const inRegion = (i) => ranges.some(([s, e]) => i >= s && i <= e);
const trim = (i) => (i >= 0 && i < lines.length ? lines[i].trim() : '');
const isControl = (t) => /^(if|for|while|switch)\s*\(/.test(t);

function findBlockEnd(openLine)
{
    let depth = 0;

    for (let j = openLine; j < lines.length; j++)
    {
        for (const ch of lines[j])
        {
            if (ch === '{')
            {
                depth++;
            }
            else if (ch === '}')
            {
                depth--;

                if (depth === 0)
                {
                    return j;
                }
            }
        }
    }

    return lines.length - 1;
}

//제어문(및 이어지는 else 체인)의 마지막 라인
function statementEnd(i)
{
    let end;

    if (trim(i).endsWith('{'))
    {
        end = findBlockEnd(i);
    }
    else if (/;\s*(\/\/.*)?$/.test(trim(i)))
    {
        end = i;
    }
    else if (trim(i + 1) === '{')
    {
        end = findBlockEnd(i + 1);
    }
    else
    {
        end = i + 1;
    }

    if (/^else\b/.test(trim(end + 1)))
    {
        return statementEnd(end + 1);
    }

    return end;
}

//3. 위반 검사 (보고만)
const violations = [];

for (let i = 0; i < lines.length; i++)
{
    if (!inRegion(i))
    {
        continue;
    }

    const t = trim(i);

    if (/^(\}\s*)?(if|for|while|switch|else|do)\b.*\{\s*$/.test(t) && !/\{\s*\}/.test(t))
    {
        violations.push(`${i + 1}: Allman 위반 - 여는 중괄호를 다음 줄로`);
    }

    //do-while 꼬리, 한 줄 가드절(if (...) return;)은 기존 코드 관례상 빈 줄 검사 제외
    if (!isControl(t) || /^while\s*\(.*\);\s*$/.test(t) || /^if\s*\(.*\)\s*(return|continue|break)\b.*;\s*(\/\/.*)?$/.test(t))
    {
        continue;
    }

    const prev = trim(i - 1);
    const prevOk = prev === '' || prev.endsWith('{') || prev.startsWith('//') || prev.startsWith('#')
        || /^(case\b.*|default\s*):$/.test(prev) || /^(public|protected|private):$/.test(prev);

    if (!prevOk)
    {
        violations.push(`${i + 1}: 제어문 앞 빈 줄 누락`);
    }

    const end = statementEnd(i);
    const next = trim(end + 1);
    const nextOk = end + 1 >= lines.length || next === '' || next.startsWith('}') || next.startsWith('#');

    if (!nextOk)
    {
        violations.push(`${end + 2}: 제어문 블록 뒤 빈 줄 누락 (${i + 1}행 ${t.split('(')[0].trim()})`);
    }
}

const fixedMsg = fixedCount > 0 ? `주석 공백 ${fixedCount}줄 자동 수정됨(파일 재확인 필요). ` : '';

if (violations.length > 0)
{
    const shown = violations.slice(0, 20).join('\n');
    process.stderr.write(`${fixedMsg}CLAUDE.md 컨벤션 위반 (${filePath}):\n${shown}\n편집한 영역만 수정할 것.`);
    process.exit(2);
}

if (fixedCount > 0)
{
    process.stdout.write(JSON.stringify({
        hookSpecificOutput: { hookEventName: 'PostToolUse', additionalContext: `${fixedMsg}(${filePath})` }
    }));
}

process.exit(0);
