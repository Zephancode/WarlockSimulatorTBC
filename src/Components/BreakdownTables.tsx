import { nanoid } from "nanoid";
import { useSelector } from "react-redux"
import { RootState } from "../redux/Store"

function formatPercentage(num: number) {
  return (Math.round(num * 10000) / 100).toFixed(2);
}

export default function BreakdownTables() {
  const breakdownObj = useSelector((state: RootState) => state.ui.combatLogBreakdown);
  const settings = useSelector((state: RootState) => state.player.settings);

  return(
    <>
      {
        settings["automatically-open-sim-details"] === 'yes' &&
          <div style={{display: breakdownObj.data.length === 0 ? 'none' : ''}}>
            <section className="breakdown-section" id="damage-breakdown-section" style={{display: 'inline-block'}}>
              <table className="breakdown-table tablesorter" id="damage-breakdown-table" data-sortlist='[[1,1]]'>
                <thead>
                  <tr>
                    <th>Name</th>
                    <th>Damage</th>
                    <th>Casts</th>
                    <th>Avg Cast</th>
                    <th>Crit %</th>
                    <th>Miss %</th>
                    <th style={{display: breakdownObj.spellDamageDict.Melee ? '' : 'none'}}>Dodge %</th>
                    <th style={{display: breakdownObj.spellDamageDict.Melee ? '' : 'none'}}>Glancing %</th>
                    <th>Dps</th>
                  </tr>
                </thead>
                <tbody>
                  {
                    breakdownObj.data
                      .filter(e => (breakdownObj.spellDamageDict[e.name] && breakdownObj.spellDamageDict[e.name] > 0))
                      .map(spell =>
                        <tr key={nanoid()} className='spell-damage-information'>
                          <td>{spell.name}</td>
                          <td><meter value={breakdownObj.spellDamageDict[spell.name] ? (breakdownObj.spellDamageDict[spell.name] / breakdownObj.totalDamageDone) * 100 : '0'} min='0' max='100'></meter> {breakdownObj.spellDamageDict[spell.name] ? formatPercentage(breakdownObj.spellDamageDict[spell.name] / breakdownObj.totalDamageDone) : 0}%</td>
                          <td className='number' title={spell.casts.toString()}>{spell.casts / breakdownObj.totalIterationAmount < 2 ? Math.round((spell.casts / breakdownObj.totalIterationAmount) * 10) / 10 : Math.round(spell.casts / breakdownObj.totalIterationAmount)}</td>
                          <td className='number'>{breakdownObj.spellDamageDict[spell.name] ? Math.round(breakdownObj.spellDamageDict[spell.name] / spell.casts) : 0}</td>
                          <td className='number'>{formatPercentage(spell.crits / spell.casts)}</td>
                          <td className='number'>{formatPercentage(spell.misses / spell.casts)}</td>
                          {
                            breakdownObj.spellDamageDict.Melee &&
                              <>
                                <td className='number'>{formatPercentage(spell.dodges / spell.casts)}</td>
                                <td className='number'>{formatPercentage(spell.glancingBlows / spell.casts)}</td>
                              </>
                          }
                          <td className='number'>{breakdownObj.spellDamageDict[spell.name] ? Math.round((breakdownObj.spellDamageDict[spell.name] / breakdownObj.totalSimulationFightLength) * 100) / 100 : 0}</td>
                        </tr>
                      )
                  }
                </tbody>
              </table>
            </section>
            <section className="breakdown-section" id="mana-gain-breakdown-section" style={{display: 'inline-block'}}>
              <table className="breakdown-table tablesorter" id="mana-gain-breakdown-table" data-sortlist='[[1,1]]'>
                <thead>
                  <tr>
                    <th>Name</th>
                    <th>% Of Total Gain</th>
                    <th>Casts</th>
                    <th>Avg Cast</th>
                  </tr>
                </thead>
                <tbody>
                  {
                    breakdownObj.data
                      .filter(e => breakdownObj.spellManaGainDict[e.name] && breakdownObj.spellManaGainDict[e.name] > 0)
                      .map(spell =>
                        <tr key={nanoid()} className='spell-damage-information'>
                          <td>{spell.name}</td>
                          <td><meter value={breakdownObj.spellManaGainDict[spell.name] ? (breakdownObj.spellManaGainDict[spell.name] / breakdownObj.totalManaGained) * 100 : '0'} min='0' max='100'></meter> {breakdownObj.spellManaGainDict[spell.name] ? formatPercentage(breakdownObj.spellManaGainDict[spell.name] / breakdownObj.totalManaGained) : 0}%</td>
                          <td className='number'>{(spell.casts / breakdownObj.totalIterationAmount) < 2 ? Math.round((spell.casts / breakdownObj.totalIterationAmount) * 10) / 10 : Math.round(spell.casts / breakdownObj.totalIterationAmount)}</td>
                          <td className='number'>{breakdownObj.spellManaGainDict[spell.name] ? Math.round(breakdownObj.spellManaGainDict[spell.name] / spell.casts) : 0}</td>
                        </tr>
                      )
                  }
                </tbody>
              </table>
            </section>
            <section className="breakdown-section" id="aura-breakdown-section" style={{display: 'inline-block'}}>
              <table className="breakdown-table tablesorter" id="aura-breakdown-table" data-sortlist='[[2,1]]'>
                <thead>
                  <tr>
                    <th>Name</th>
                    <th>Count</th>
                    <th>Uptime</th>
                  </tr>
                </thead>
                <tbody>
                  {
                    breakdownObj.data.filter(e => e.uptime > 0).map(spell =>
                      <tr key={nanoid()} className='spell-damage-information'>
                        <td>{spell.name}</td>
                        {/*If the aura's count is less than 2 then show 1 decimal place, otherwise just round the value*/}
                        <td className='number'>{(spell.count / breakdownObj.totalIterationAmount) < 2 ? Math.round((spell.count / breakdownObj.totalIterationAmount) * 10) / 10 : Math.round(spell.count / breakdownObj.totalIterationAmount)}</td>
                        <td><meter value={(spell.uptime / breakdownObj.totalSimulationFightLength) * 100} min='0' max='100'></meter> {formatPercentage(spell.uptime / breakdownObj.totalSimulationFightLength)}%</td>
                      </tr>
                    )
                  }
                </tbody>
              </table>
            </section>
          </div>
      }
    </>
  )
}