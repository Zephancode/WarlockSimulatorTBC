class Simulation {
	static getSettings() {
		return {
			"iterations": parseInt($("input[name='iterations']").val()),
			"minTime": parseInt($("input[name='min-fight-length']").val()),
			"maxTime": parseInt($("input[name='max-fight-length']").val())
		}
	}

	constructor(player, settings = Simulation.getSettings()) {
		this.player = player;
		this.iterations = settings.iterations;
		this.minTime = settings.minTime;
		this.maxTime = settings.maxTime;
	}

	passTime() {
		let time = this.player.castTimeRemaining;
		if (time == 0) time = Math.max(0.1,this.player.gcdRemaining);

		if (this.player.auras.improvedShadowBolt && this.player.auras.improvedShadowBolt.active && this.player.auras.improvedShadowBolt.durationRemaining < time) time = this.player.auras.improvedShadowBolt.durationRemaining;
		if (this.player.auras.corruption && this.player.auras.corruption.active && this.player.auras.corruption.tickTimerRemaining < time) time = this.player.auras.corruption.tickTimerRemaining; 
		if (this.player.auras.unstableAffliction && this.player.auras.unstableAffliction.active && this.player.auras.unstableAffliction.tickTimerRemaining < time) time = this.player.auras.unstableAffliction.tickTimerRemaining;
		if (this.player.auras.siphonLife && this.player.auras.siphonLife.active && this.player.auras.siphonLife.tickTimerRemaining < time) time = this.player.auras.siphonLife.tickTimerRemaining;
		if (this.player.auras.immolate && this.player.auras.immolate.active && this.player.auras.immolate.tickTimerRemaining < time) time = this.player.auras.immolate.tickTimerRemaining;
		if (this.player.mp5Timer < time) time = this.player.mp5Timer;

		// This needs to be the first modified value since the time in combat needs to be updated before spells start dealing damage/auras expiring etc. for the combat logging.
		this.player.fightTime += time;
		this.player.castTimeRemaining = Math.max(0,this.player.castTimeRemaining - time);

		// Spells
		for (let spell in this.player.spells) {
			let damage = this.player.spells[spell].tick(time);
			if (damage > 0) {
				this.player.iterationDamage += damage;
			}
		}

		// Auras
		for (let aura in this.player.auras) {
			this.player.auras[aura].tick(time);
		}

		this.player.gcdRemaining = Math.max(0,this.player.gcdRemaining - time);
		this.player.mp5Timer = Math.max(0,this.player.mp5Timer - time);
		if (this.player.mp5Timer == 0 && this.player.stats.mp5 > 0) {
			this.player.mana = Math.min(this.player.stats.maxMana, this.player.mana + this.player.stats.mp5);
			this.player.mp5Timer = 5;
			this.player.combatLog(this.player.stats.mp5 + " mana gained from MP5");
		}
	}

	start() {
		let totalDamage = 0;
		let totalDuration = 0;
		let minDps = 9999;
		let maxDps = 0;
		let startTime = new Date();

		console.log("------- Simualtion start -------");
		for(this.player.iteration = 1; this.player.iteration <= this.iterations; this.player.iteration++) {
			this.player.initialize();
			this.player.iterationDamage = 0;
			let fightLength = this.player.random(this.minTime, this.maxTime);

			for(this.player.fightTime = 0; this.player.fightTime < fightLength; this.passTime()) {
				if (this.player.castTimeRemaining <= 0) {
					if (this.player.gcdRemaining <= 0) {
						if (this.player.rotation.dots.unstableAffliction && !this.player.auras.unstableAffliction.active && this.player.spells.unstableAffliction.ready()) {
							this.player.cast("unstableAffliction");
						} else if (this.player.rotation.dots.corruption && !this.player.auras.corruption.active && this.player.spells.corruption.ready()) {
							this.player.cast("corruption");
						} else if (this.player.rotation.dots.siphonLife && !this.player.auras.siphonLife.active && this.player.spells.siphonLife.ready()) {
							this.player.cast("siphonLife");
						} else if (this.player.rotation.dots.immolate && !this.player.auras.immolate.active && this.player.spells.immolate.ready()) {
							this.player.cast("immolate");
						} else if (this.player.spells[this.player.filler].ready()) {
							this.player.cast(this.player.filler);
						} else {
							this.player.cast("lifeTap");
						}
					}
				}
			}

			totalDuration += fightLength;
			totalDamage += this.player.iterationDamage;

			let iterationDps = this.player.iterationDamage / fightLength;
			if (iterationDps > maxDps) maxDps = iterationDps;
			else if (iterationDps < minDps) minDps = iterationDps;
		}

		console.log('------- Simulation end -------');
		localStorage['avgDps'] = Math.round((totalDamage / totalDuration) * 100) / 100;
		localStorage['minDps'] = Math.round(minDps * 100) / 100;
		localStorage['maxDps'] = Math.round(maxDps * 100) / 100;
		localStorage['simulationDuration'] = (new Date() - startTime) / 1000;
		$("#avg-dps").text(localStorage['avgDps']);
		$("#min-dps").text(localStorage['minDps']);
		$("#max-dps").text(localStorage['maxDps']);
		$("#sim-length-result").text(localStorage['simulationDuration'] + "s");

		$(".spell-damage-information").remove();
		let tableBody = $("#damage-breakdown-table tbody");
		for (let spell of Object.keys(this.player.damageBreakdown)) {
			let s = this.player.damageBreakdown[spell];
			if (s.damage > 0 || s.casts > 0) tableBody.append("<tr class='spell-damage-information'><td>" + s.name + "</td><td>" + (~~(((s.damage / totalDamage) * 100) * 100) / 100).toFixed(2) + "%" + "</td><td>" + s.casts + "</td><td>" + ~~(s.damage / s.casts) + (s.dotDamage ? ("(" + ~~(s.dotDamage / s.casts) + ")") : "") + "</td><td>" + ((~~(((s.crits / s.casts) * 100) * 100)) / 100).toFixed(2) + "</td><td>" + (~~(((s.misses / s.casts) * 100) * 100) / 100).toFixed(2) + "</td></tr>");
		}
		$("#damage-breakdown-table").css("visibility", "visible");
		$("#damage-breakdown-section h3").css("visibility", 'visible');
	}
}