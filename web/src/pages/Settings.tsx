import { Alert, Card, Code, SimpleGrid, Stack, Table, Text, TextInput, Title } from '@mantine/core';
import { IconSearch } from '@tabler/icons-react';
import { useState } from 'react';
import { api, Settings } from '../api';
import { settingValue } from '../format';
import { usePolling } from '../hooks';
import { useI18n } from '../i18n';

// Settings sorted by name, only the ones with the filter in their name
function SettingsTable({ settings, filter }: { settings: Settings; filter: string }) {
  const rows = Object.entries(settings)
    .filter(([name]) => name.toLowerCase().includes(filter))
    .sort(([a], [b]) => a.localeCompare(b));
  return (
    <Table.ScrollContainer minWidth={300}>
      <Table striped verticalSpacing={4}>
        <Table.Tbody>
          {rows.map(([name, value]) => (
            <Table.Tr key={name}>
              <Table.Td w="1%" style={{ whiteSpace: 'nowrap' }}>
                <Code>{name}</Code>
              </Table.Td>
              <Table.Td style={{ overflowWrap: 'anywhere' }}>{settingValue(value)}</Table.Td>
            </Table.Tr>
          ))}
        </Table.Tbody>
      </Table>
    </Table.ScrollContainer>
  );
}

export function SettingsPage() {
  const { t } = useI18n();
  const { data, error } = usePolling(api.settingsState, 5000);
  const [filter, setFilter] = useState('');

  if (error) return <Alert color="red">{error.message}</Alert>;
  if (!data) return null;
  const { stats, ...session } = data.session;
  const search = filter.trim().toLowerCase();

  const sections: [string, Settings, string?][] = [
    [t('settings.session'), session],
    [t('settings.lt2http'), data.lt2http, t('settings.secrets_hidden')],
    [t('settings.libtorrent'), data.libtorrent],
    [t('settings.stats'), stats],
  ];

  return (
    <Stack>
      <Title order={3}>{t('nav.settings')}</Title>
      <TextInput
        placeholder={t('settings.filter')}
        leftSection={<IconSearch size={16} />}
        value={filter}
        onChange={(e) => setFilter(e.currentTarget.value)}
        maw={400}
      />
      <SimpleGrid cols={{ base: 1, lg: 2 }}>
        {sections.map(([title, settings, hint]) => (
          <Card withBorder key={title}>
            <Title order={5}>{title}</Title>
            {hint && (
              <Text size="xs" c="dimmed">
                {hint}
              </Text>
            )}
            <SettingsTable settings={settings} filter={search} />
          </Card>
        ))}
      </SimpleGrid>
    </Stack>
  );
}
