import { Anchor, AppShell, Burger, Group, NativeSelect, NavLink, Text } from '@mantine/core';
import { useDisclosure } from '@mantine/hooks';
import { IconApi, IconInfoCircle, IconList, IconPlus, IconSettings } from '@tabler/icons-react';
import { useHashRoute } from './hooks';
import { useI18n } from './i18n';
import { AddTorrent } from './pages/AddTorrent';
import { SettingsPage } from './pages/Settings';
import { TorrentDetails } from './pages/TorrentDetails';
import { TorrentList } from './pages/TorrentList';

export function App() {
  const { t, language, languages, setLanguage } = useI18n();
  const [page, arg] = useHashRoute();
  const [opened, { toggle, close }] = useDisclosure();

  const links = [
    { href: '#/', label: t('nav.torrents'), icon: IconList, active: !page || page === 'torrent' },
    { href: '#/add', label: t('nav.add'), icon: IconPlus, active: page === 'add' },
    { href: '#/settings', label: t('nav.settings'), icon: IconSettings, active: page === 'settings' },
  ];

  let content;
  if (page === 'torrent' && arg) content = <TorrentDetails hash={arg} />;
  else if (page === 'add') content = <AddTorrent />;
  else if (page === 'settings') content = <SettingsPage />;
  else content = <TorrentList />;

  return (
    <AppShell
      header={{ height: 56 }}
      navbar={{ width: 220, breakpoint: 'sm', collapsed: { mobile: !opened } }}
      padding="md"
    >
      <AppShell.Header>
        <Group h="100%" px="md" justify="space-between" wrap="nowrap">
          <Group gap="sm" wrap="nowrap">
            <Burger opened={opened} onClick={toggle} hiddenFrom="sm" size="sm" />
            <Anchor href="#/" underline="never" c="inherit">
              <Text fw={700} size="lg">
                lt2http
              </Text>
            </Anchor>
          </Group>
          <NativeSelect
            aria-label={t('ui.language')}
            value={language}
            onChange={(event) => setLanguage(event.currentTarget.value)}
            data={languages.map((l) => ({ value: l.code, label: l.name }))}
            w={200}
          />
        </Group>
      </AppShell.Header>

      <AppShell.Navbar p="sm">
        {links.map((link) => (
          <NavLink
            key={link.href}
            href={link.href}
            label={link.label}
            leftSection={<link.icon size={18} />}
            active={link.active}
            onClick={close}
          />
        ))}
        <NavLink href="/info" target="_blank" label="/info" leftSection={<IconInfoCircle size={18} />} />
        <NavLink href="/swagger/ui" target="_blank" label="API" leftSection={<IconApi size={18} />} />
      </AppShell.Navbar>

      <AppShell.Main>{content}</AppShell.Main>
    </AppShell>
  );
}
