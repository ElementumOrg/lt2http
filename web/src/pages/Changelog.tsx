import { Alert, Card, Code, List, Stack, Text, Title } from '@mantine/core';
import { ReactNode } from 'react';
import { api } from '../api';
import { usePolling } from '../hooks';
import { useI18n } from '../i18n';

interface Release {
  title: string;
  // Bullet items, other lines become paragraphs
  lines: { text: string; isItem: boolean }[];
}

// changelog.md has "## <version> (<date>)" headings with "- " items, nothing more is needed
export function parseChangelog(markdown: string): Release[] {
  const releases: Release[] = [];
  for (const line of markdown.split('\n')) {
    if (line.startsWith('## ')) releases.push({ title: line.slice(3).trim(), lines: [] });
    else if (releases.length && line.startsWith('- '))
      releases[releases.length - 1].lines.push({ text: line.slice(2), isItem: true });
    else if (releases.length && line.trim())
      releases[releases.length - 1].lines.push({ text: line.trim(), isItem: false });
  }
  return releases;
}

// `code` in the text
function Inline({ text }: { text: string }) {
  const parts: ReactNode[] = text
    .split(/(`[^`]+`)/)
    .map((part, i) => (part.startsWith('`') && part.endsWith('`') ? <Code key={i}>{part.slice(1, -1)}</Code> : part));
  return <>{parts}</>;
}

export function ChangelogPage() {
  const { t } = useI18n();
  // Changes only with a new lt2http, no need to load it again
  const { data, error } = usePolling(api.changelog, 0);

  if (error) return <Alert color="red">{error.message}</Alert>;
  if (!data) return null;

  return (
    <Stack maw={1000}>
      <Title order={3}>{t('nav.changelog')}</Title>
      {parseChangelog(data).map((release) => (
        <Card withBorder key={release.title}>
          <Title order={4} mb="sm">
            {release.title}
          </Title>
          <List spacing={6} size="sm">
            {release.lines.map((line, i) =>
              line.isItem ? (
                <List.Item key={i}>
                  <Inline text={line.text} />
                </List.Item>
              ) : (
                <Text key={i} size="sm">
                  <Inline text={line.text} />
                </Text>
              ),
            )}
          </List>
        </Card>
      ))}
    </Stack>
  );
}
