import {
  ActionIcon,
  Anchor,
  Badge,
  Card,
  Code,
  CopyButton,
  SimpleGrid,
  Stack,
  Table,
  Text,
  Title,
  Tooltip,
} from '@mantine/core';
import { IconCheck, IconCopy } from '@tabler/icons-react';
import { ReactNode } from 'react';
import { NetworkState } from '../api';
import { useI18n } from '../i18n';

function Copy({ value }: { value: string }) {
  const { t } = useI18n();
  return (
    <CopyButton value={value}>
      {({ copied, copy }) => (
        <Tooltip label={t(copied ? 'ui.link_copied' : 'ui.copy_link')}>
          <ActionIcon variant="subtle" color={copied ? 'green' : 'gray'} onClick={copy} aria-label={t('ui.copy_link')}>
            {copied ? <IconCheck size={16} /> : <IconCopy size={16} />}
          </ActionIcon>
        </Tooltip>
      )}
    </CopyButton>
  );
}

// A table with a header, or a text when there are no rows
function List({ head, rows, empty }: { head: string[]; rows: ReactNode[][]; empty: string }) {
  if (!rows.length)
    return (
      <Text size="sm" c="dimmed">
        {empty}
      </Text>
    );
  return (
    <Table.ScrollContainer minWidth={300}>
      <Table verticalSpacing={4}>
        <Table.Thead>
          <Table.Tr>
            {head.map((h) => (
              <Table.Th key={h}>{h}</Table.Th>
            ))}
          </Table.Tr>
        </Table.Thead>
        <Table.Tbody>
          {rows.map((row, i) => (
            <Table.Tr key={i}>
              {row.map((cell, j) => (
                <Table.Td key={j} style={{ overflowWrap: 'anywhere' }}>
                  {cell}
                </Table.Td>
              ))}
            </Table.Tr>
          ))}
        </Table.Tbody>
      </Table>
    </Table.ScrollContainer>
  );
}

// Links to lt2http from other devices, listen sockets and port mappings
export function NetworkCard({ network }: { network: NetworkState }) {
  const { t } = useI18n();
  const link = (address: string, path: string) => `http://${address}:${network.web_port}${path}`;

  return (
    <Card withBorder>
      <Title order={5} mb="sm">
        {t('settings.network')}
      </Title>
      <Stack>
        <div>
          <Text fw={500} size="sm" mb={4}>
            {t('network.links')}
          </Text>
          <List
            head={[t('network.address'), 'Web UI', '/info']}
            empty="-"
            rows={network.web_addresses.map((address) => [
              address,
              <LinkWithCopy key="web" link={link(address, '/web/')} />,
              <LinkWithCopy key="info" link={link(address, '/info')} />,
            ])}
          />
        </div>

        <SimpleGrid cols={{ base: 1, lg: 3 }} spacing="lg">
          <div>
            <Text fw={500} size="sm" mb={4}>
              {t('network.listen')}
            </Text>
            <Text size="xs" c="dimmed" mb={4}>
              {t('network.listen_interfaces')}: <Code>{network.listen_interfaces}</Code>
            </Text>
            <List
              head={[t('network.address'), t('network.port'), t('network.type'), t('label.status')]}
              empty={t('network.no_mappings')}
              rows={network.listen_sockets.map((s) => [
                s.address,
                s.port,
                s.type,
                s.error ? (
                  <Text size="xs" c="red">
                    {s.error}
                  </Text>
                ) : (
                  <Badge color="green" variant="light">
                    {t('network.listening')}
                  </Badge>
                ),
              ])}
            />
          </div>

          <div>
            <Text fw={500} size="sm" mb={4}>
              {t('network.natpmp')}{' '}
              <Badge color={network.natpmp_enabled ? 'green' : 'gray'} variant="light">
                {t(network.natpmp_enabled ? 'network.natpmp_on' : 'network.natpmp_off')}
              </Badge>
            </Text>
            <List
              head={[t('network.interface'), t('network.local'), t('network.gateway'), t('network.port')]}
              empty={t('network.no_mappings')}
              rows={network.natpmp_mappings.map((m) => [m.interface, m.local, m.gateway, m.port])}
            />
          </div>

          <div>
            <Text fw={500} size="sm" mb={4}>
              {t('network.port_mappings')}
            </Text>
            <List
              head={[t('network.transport'), t('network.local_port'), t('label.message')]}
              empty={t('network.no_mappings')}
              rows={network.port_mappings.map((m) => [
                m.transport,
                m.local_port || '-',
                <Text key="message" size="xs" c={m.is_error ? 'red' : undefined}>
                  {m.message}
                </Text>,
              ])}
            />
          </div>
        </SimpleGrid>
      </Stack>
    </Card>
  );
}

// A link with its copy button
function LinkWithCopy({ link }: { link: string }) {
  return (
    <span style={{ display: 'inline-flex', alignItems: 'center', gap: 4 }}>
      <Anchor href={link} target="_blank" size="sm">
        {link}
      </Anchor>
      <Copy value={link} />
    </span>
  );
}
